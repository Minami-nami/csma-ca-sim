#pragma once
#include "DataWarper.pb.h"
#include "ThreadPool.h"
#include "qdatetime.h"
#include "qtmetamacros.h"
#include "ui_Node.h"
#include "window_node.h"
#include <QApplication>
#include <QMessageBox>
#include <QThread>
#include <QTime>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/bind/bind.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <shared_mutex>
#include <thread>

enum class status {
    free,
    busy,
    conflict,
};
class ThreadReceive_Node;
class ThreadSend_Node;

class Process_Node : public QApplication {
    Q_OBJECT
public:
    using address_t          = boost::asio::ip::address;
    using socket_t           = boost::asio::ip::tcp::socket;
    using endpoint_t         = boost::asio::ip::tcp::endpoint;
    using socket_ptr_t       = std::shared_ptr<socket_t>;
    using mac_t              = std::byte;
    using thread_t           = std::thread;
    using mutex_t            = std::mutex;
    using shared_mutex_t     = std::shared_mutex;
    using shared_lock_t      = std::shared_lock<shared_mutex_t>;
    using unique_lock_t      = std::unique_lock<shared_mutex_t>;
    using port_t             = std::uint16_t;
    using io_context_t       = boost::asio::io_context;
    using pool_ptr_t         = std::unique_ptr<ThreadPool>;
    using opinion_t          = boost::asio::ip::tcp::socket::reuse_address;
    using queue_t            = std::queue<DataWarper>;
    using message_func_ptr_t = QMessageBox::StandardButton (*)(QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);
    bool connected_;

private:
    Window_Node window_;
    address_t   address_current_;
    address_t   address_server_;
    port_t      port_server_;
    port_t      port_current_;

    socket_ptr_t socket_client_;

    mac_t mac_current_;

    shared_mutex_t queue_lock_;
    shared_mutex_t status_lock_;
    shared_mutex_t time_lock_;
    status         status_;

    io_context_t io_context_;

    opinion_t opinion_;

    queue_t queue_;

    uint16_t backoff_counter_;

    int backoff_time_;

    QTime transform_time_;

    static constexpr size_t      bufsize = 1024;
    char                         buffer[bufsize];
    static constexpr std::size_t max_thread = 4;

    ThreadReceive_Node *thread_receive_;
    ThreadSend_Node    *thread_send_;

private:
signals:

public:
    Process_Node(int &argc, char **argv);
    ~Process_Node();
    status getstatus() {
        shared_lock_t lock(status_lock_);
        return status_;
    }
    void setstatus(status status) {
        unique_lock_t lock(status_lock_);
        status_ = status;
    }
    QTime gettime() {
        shared_lock_t lock(time_lock_);
        return transform_time_;
    }
    void settime(QTime time) {
        unique_lock_t lock(time_lock_);
        transform_time_ = time;
    }
    bool isQueueEmpty() {
        shared_lock_t lock(queue_lock_);
        return queue_.empty();
    }
    void pushQueue(const DataWarper &data) {
        unique_lock_t lock(queue_lock_);
        queue_.push(data);
    }
    void popQueue() {
        unique_lock_t lock(queue_lock_);
        queue_.pop();
    }
    void cleanQueue() {
        unique_lock_t lock(queue_lock_);
        queue_t       empty;
        queue_.swap(empty);
    }
    const DataWarper &frontQueue() {
        shared_lock_t lock(queue_lock_);
        return queue_.front();
    }
};

constexpr int timelast   = 8000;
constexpr int DIFS       = 2000;  // 单位 ms
constexpr int SIFS       = 1000;  // 单位 ms
constexpr int maxBackoff = 5;
constexpr int buffsize   = 1024;

class ThreadReceive_Node : public QThread {
    Q_OBJECT
public:
    using address_t          = boost::asio::ip::address;
    using socket_t           = boost::asio::ip::tcp::socket;
    using endpoint_t         = boost::asio::ip::tcp::endpoint;
    using socket_ptr_t       = std::shared_ptr<socket_t>;
    using mac_t              = std::byte;
    using thread_t           = std::thread;
    using mutex_t            = std::mutex;
    using port_t             = std::uint16_t;
    using io_context_t       = boost::asio::io_context;
    using opinion_t          = boost::asio::ip::tcp::socket::reuse_address;
    using queue_t            = std::queue<DataWarper>;
    using message_func_ptr_t = QMessageBox::StandardButton (*)(QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);

protected:
    void run() override {
        qDebug() << "starting receive";
        using boost::asio::placeholders::error;
        io_context_.reset();
        socket_client_->async_receive(boost::asio::buffer(buffer), boost::bind(&ThreadReceive_Node::receiveHandler, this, error, boost::asio::placeholders::bytes_transferred));
        io_context_.run();
        qDebug() << "ending receive";
        quit();
    }

public:
    ThreadReceive_Node(Process_Node &node, Window_Node &window, socket_ptr_t socket_client, io_context_t &io_context, mac_t &mac_current, QObject *parent = nullptr)
        : app_(node), window_(window), socket_client_(socket_client), io_context_(io_context), mac_current_(mac_current), QThread(parent) {
        qRegisterMetaType<message_func_ptr_t>("message_func_ptr_t");
        io_context_.reset();
        QObject::connect(this, &ThreadReceive_Node::messageBoxSignal, &app_,
                         [this](message_func_ptr_t func, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons button, QMessageBox::StandardButton nobutton) {
                             func(parent, title, text, button, nobutton);
                         });
        QObject::connect(this, &ThreadReceive_Node::setText, &app_, [this](const QString &str) { window_.ui_->text_Receive->setText(str); });
    }
    ~ThreadReceive_Node() {
        boost::system::error_code ec;
        wait();
        qDebug() << "deconstructing receive";
        if (socket_client_ && socket_client_->is_open()) socket_client_->close();

        socket_client_.reset();
        io_context_.stop();
        app_.connected_ = false;
        qDebug() << "deconstructed receive";
        emit taskend();
    }

private:
    void receiveHandler(const boost::system::error_code &ec, size_t bytes_transferred) {
        using boost::asio::placeholders::error;
        if (ec) {
            emit messageBoxSignal(static_cast<message_func_ptr_t>(&QMessageBox::critical), window_.ui_->Widget, QString::fromStdString("Error"), QString::fromStdString(ec.message()),
                                  QMessageBox::StandardButtons(QMessageBox::Ok), QMessageBox::NoButton);
            return;
        }
        DataWarper data;
        data.ParseFromArray(buffer, bytes_transferred);
        if (data.type() == DataWarper::ackFrame) {
            app_.pushQueue(data);
        }
        else if (data.type() == DataWarper::announceFrame) {
            qDebug() << static_cast<char>(mac_current_)
                     << QString("receive announce of %1 at [%2]").arg(static_cast<char>(data.announceframebody().nodesend())).arg(QTime::currentTime().toString("hh:mm:ss.zzz"));
            app_.setstatus(status::busy);
            qDebug() << "set status to busy";
            app_.settime(QTime::currentTime());
            qDebug() << "setTime to " << app_.gettime().toString("hh:mm:ss.zzz");
        }
        else if (data.type() == DataWarper::conflictFrame) {
            qDebug() << static_cast<char>(mac_current_)
                     << QString("receive conflict of %1 and %2 at [%3]")
                            .arg(static_cast<char>(data.conflictframebody().nodesend1()))
                            .arg(static_cast<char>(data.conflictframebody().nodesend2()))
                            .arg(QTime::currentTime().toString("hh:mm:ss.zzz"));
            app_.setstatus(status::conflict);
            qDebug() << "set status to conflict";
            app_.settime(QTime::currentTime());
            qDebug() << "setTime to " << app_.gettime().toString("hh:mm:ss.zzz");
        }
        socket_client_->async_receive(boost::asio::buffer(buffer), boost::bind(&ThreadReceive_Node::receiveHandler, this, error, boost::asio::placeholders::bytes_transferred));
    }

private:
    Process_Node &app_;
    Window_Node  &window_;
    socket_ptr_t  socket_client_;
    io_context_t &io_context_;
    mac_t        &mac_current_;
    char          buffer[buffsize];

public:
signals:
    void taskend();
    void setText(const QString &);
    void messageBoxSignal(message_func_ptr_t func, QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);
};

class ThreadSend_Node : public QThread {
    Q_OBJECT
public:
    using address_t          = boost::asio::ip::address;
    using socket_t           = boost::asio::ip::tcp::socket;
    using endpoint_t         = boost::asio::ip::tcp::endpoint;
    using socket_ptr_t       = std::shared_ptr<socket_t>;
    using mac_t              = std::byte;
    using thread_t           = std::thread;
    using mutex_t            = std::mutex;
    using port_t             = std::uint16_t;
    using io_context_t       = boost::asio::io_context;
    using opinion_t          = boost::asio::ip::tcp::socket::reuse_address;
    using queue_t            = std::queue<DataWarper>;
    using message_func_ptr_t = QMessageBox::StandardButton (*)(QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);

protected:
    void run() override {
        DataWarper                data;
        boost::system::error_code ec;
        qDebug() << "starting send";
        std::string str_ = window_.ui_->text_Send->toPlainText().toStdString();
        mac_current_     = static_cast<std::byte>(window_.ui_->comboBox_MAC->currentText()[0].toLatin1());

        data.set_type(DataWarper::dataFrame);

        header *header = data.mutable_header();
        header->set_destmac(static_cast<google::protobuf::uint32>(/*TODO*/ 0));
        header->set_srcmac(static_cast<google::protobuf::uint32>(mac_current_));

        dataFrameBody *body = data.mutable_dataframebody();
        // 封装一个数据帧
        body->set_data(str_);
        body->set_length(str_.length());
        body->set_timelast(static_cast<google::protobuf::uint32>(timelast));
        backoff_counter_ = 0;
        backoff_time_    = 0;
    BACKOFF_START:
        // 计算退避时间
        // 设置退避时间
        if (backoff_counter_ == 0)
            backoff_time_ = 0;
        else {
            backoff_time_ = mt_rand() % (1 << (backoff_counter_ + 2));
            backoff_time_ *= SIFS;
        }
        // 查看信道状态
    JUDGE_STATUS:
        switch (app_.getstatus()) {
        case status::conflict:
            emit setText(QString("Conflict"));
            app_.setstatus(status::free);
        FREE:
        case status::free:
            // 等待DIFS时间
            std::this_thread::sleep_for(std::chrono::milliseconds(DIFS));
        BACKOFF:
            // 退避时间>0
            emit setText(QString("Backoff time: %1 ms").arg(backoff_time_));
            if (backoff_time_ > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 等待0.1s
            else
                goto SEND;
            if (app_.getstatus() == status::free) {  // 信道空闲
                // 退避时间减0.1s
                backoff_time_ -= 100;
                // 设置退避时间
                goto BACKOFF;
            }
            goto JUDGE_STATUS;
        case status::busy:
            qDebug() << static_cast<char>(mac_current_) << " time of change to busy: " << app_.gettime().toString("hh:mm:ss.zzz");
            qDebug() << static_cast<char>(mac_current_) << " now time: " << QTime::currentTime().toString("hh:mm:ss.zzz");

            if (app_.gettime().msecsTo(QTime::currentTime()) >= 8000) {  // 信道为空闲状态
                app_.setstatus(status::free);
                goto FREE;
            }
            // 信道忙碌
            emit setText(QString("Busy"));
            // 等待信道空闲
            std::this_thread::sleep_for(std::chrono::milliseconds(-app_.gettime().msecsTo(QTime::currentTime()) + 8000));
            goto JUDGE_STATUS;
        }

    SEND:
        // 发送数据帧
        app_.cleanQueue();
        socket_client_->send(boost::asio::buffer(data.SerializeAsString()), 0, ec);
        emit setText(QString("waiting for ACK"));
        if (ec) return;
        qDebug() << static_cast<char>(mac_current_) << " successfully send at " << QTime::currentTime().toString("hh:mm:ss.zzz");
        // 等待ap回复
        while (socket_client_->is_open()) {
            if (!app_.isQueueEmpty()) {
                auto &&data = app_.frontQueue();
                app_.popQueue();
                // 收到ACK帧
                if (data.type() == DataWarper::ackFrame) goto END;
            }
            if (app_.getstatus() == status::conflict) {
                // 收到冲突通告
                backoff_counter_++;
                if (backoff_counter_ > maxBackoff) {
                    emit messageBoxSignal(static_cast<message_func_ptr_t>(QMessageBox::critical), window_.ui_->Widget, QString::fromStdString("Error"), QString::fromStdString("Time Out"),
                                          QMessageBox::StandardButtons(QMessageBox::Ok), QMessageBox::NoButton);
                    emit setText(QString("Time out"));
                    qDebug() << "send error";
                    setSending(false);
                    quit();
                    return;
                }
                goto BACKOFF_START;
            }
        }
    END:
        emit setText(QString("Send successfully"));
        qDebug() << "ending send";
        setSending(false);
        try {
            quit();
            exec();
        }
        catch (boost::system::system_error &e) {
            qDebug() << e.what();
        }
        catch (std::exception &e) {
            qDebug() << e.what();
        }
        catch (...) {
            qDebug() << "unknown error";
        }
        return;
    }

public:
    ThreadSend_Node(Window_Node &window, Process_Node &node, socket_ptr_t socket_client, mac_t mac_current, QObject *parent = nullptr)
        : backoff_time_(0), backoff_counter_(0), window_(window), sending_(false), app_(node), socket_client_(socket_client), mac_current_(mac_current), QThread(parent) {
        qRegisterMetaType<message_func_ptr_t>("message_func_ptr_t");
        QObject::connect(this, &ThreadSend_Node::messageBoxSignal, &app_,
                         [this](message_func_ptr_t func, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons button, QMessageBox::StandardButton nobutton) {
                             func(parent, title, text, button, nobutton);
                         });
        QObject::connect(this, &ThreadSend_Node::setText, &app_, [this](const QString &str) { window_.ui_->text_Receive->setText(str); });
    }
    ~ThreadSend_Node() {
        wait();
        qDebug() << "deconstructing send";
        socket_client_.reset();
        qDebug() << "deconstructed send";
        emit taskend();
    }
    bool getSending() {
        std::lock_guard<mutex_t> lock(sending_lock_);
        return sending_;
    }
    void setSending(bool sending) {
        std::lock_guard<mutex_t> lock(sending_lock_);
        sending_ = sending;
    }

private:
    int          backoff_time_;
    Window_Node &window_;

    bool    sending_;
    mutex_t sending_lock_;
    mac_t   mac_current_;

    socket_ptr_t  socket_client_;
    uint16_t      backoff_counter_;
    Process_Node &app_;
    std::mt19937  mt_rand{ std::random_device{}() };

public:
signals:
    void setText(const QString &);
    void messageBoxSignal(message_func_ptr_t func, QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);
    void taskend();
};