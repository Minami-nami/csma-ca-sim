#pragma once
#include "DataWarper.pb.h"
#include "NodeInfo.h"
#include "NodeStatus.h"
#include "qdatetime.h"
#include "qmessagebox.h"
#include "qobject.h"
#include "qthread.h"
#include "qtmetamacros.h"
#include "ui_AP.h"
#include "window_ap.h"
#include <QMessageBox>
#include <QThread>
#include <QTime>
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/bind/bind.hpp>
#include <boost/system/error_code.hpp>
#include <mutex>
#include <shared_mutex>
#include <thread>

class ReceiveThread_AP : public QThread {
    Q_OBJECT
public:
    using address_t          = boost::asio::ip::address;
    using port_t             = uint16_t;
    using acceptor_t         = boost::asio::ip::tcp::acceptor;
    using socket_t           = boost::asio::ip::tcp::socket;
    using del_t              = std::function<void(acceptor_t *)>;
    using acceptor_ptr_t     = std::unique_ptr<acceptor_t, del_t>;
    using socket_ptr_t       = std::shared_ptr<socket_t>;
    using endpoint_t         = boost::asio::ip::tcp::endpoint;
    using io_context_t       = boost::asio::io_context;
    using thread_t           = std::thread;
    using thread_ptr_t       = std::unique_ptr<thread_t>;
    using mac_t              = std::byte;
    using message_func_ptr_t = QMessageBox::StandardButton (*)(QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);
    using mutex_t            = std::mutex;
    using shared_mutex_t     = std::shared_mutex;
    using shared_lock_t      = std::shared_lock<shared_mutex_t>;
    using unique_lock_t      = std::unique_lock<shared_mutex_t>;

public:
    explicit ReceiveThread_AP(socket_ptr_t socket_server, shared_mutex_t &log_mutex, shared_mutex_t &status_mutex, NodeStatus &status, Window_AP &window, QObject *parent = nullptr)
        : socket_server_(socket_server), log_mutex_(log_mutex), status_mutex_(status_mutex), status_(status), window_(window), QThread(parent) {
        qRegisterMetaType<message_func_ptr_t>("message_func_ptr_t");
        QObject::connect(this, &ReceiveThread_AP::showStatusSignal, &window_, [this](const NodeInfo &nodeinfo) { showStatus(nodeinfo); });
        QObject::connect(this, &ReceiveThread_AP::messageBoxSignal, &window_,
                         [this](message_func_ptr_t func, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
                             func(parent, title, text, buttons, defaultButton);
                         });
        QObject::connect(this, &ReceiveThread_AP::appendLogSignal, &window_, [this](const QString &text) {
            unique_lock_t lock(log_mutex_);
            window_.ui_->textBrowser_log->append(text);
        });
    }
    ~ReceiveThread_AP() {
        if (socket_server_->is_open()) socket_server_->close();
        socket_server_.reset();
        if (mac_current_ != mac_t{ 0 }) status_.removeNode(mac_current_);
    }

private:
    socket_ptr_t    socket_server_;
    shared_mutex_t &log_mutex_;
    shared_mutex_t &status_mutex_;
    NodeStatus     &status_;
    Window_AP      &window_;
    mac_t           mac_current_ = mac_t{ 0 };

protected:
    void run() override {
        DataWarper                data;
        boost::system::error_code ec;
        QTime                     receive_time;
        while (socket_server_->is_open()) {
            constexpr size_t buffer_size = 1024;
            char             buffer[buffer_size];
            int              total_length = 0;
            // 接收一个数据帧
            int readed = socket_server_->receive(boost::asio::buffer(buffer), 0, ec);
            if (ec) {
                if (ec == boost::system::errc::bad_file_descriptor) return;
                if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset) {
                    emit appendLogSignal(QString("与 [%1, %2]断开连接. [%3]")
                                             .arg(socket_server_->remote_endpoint().address().to_string().c_str())
                                             .arg(socket_server_->remote_endpoint().port())
                                             .arg(QTime::currentTime().toString("hh:mm:ss")));
                    return;
                }
                emit messageBoxSignal(static_cast<message_func_ptr_t>(QMessageBox::critical), window_.ui_->Widget, "Error", QString::fromStdString(ec.message()), QMessageBox::Ok,
                                      QMessageBox::NoButton);
                return;
            }
            DataWarper warpped_data;
            // 解析数据帧
            warpped_data.ParseFromArray(buffer, readed);
            auto  mac_dest        = static_cast<std::byte>(warpped_data.header().destmac());
            auto  mac_src         = static_cast<std::byte>(warpped_data.header().srcmac());
            auto  client_endpoint = socket_server_->remote_endpoint();
            mac_t mac_conflict;
            mac_current_ = mac_src;
            emit appendLogSignal(QString("收到 %1 的数据帧, 内容为: %2. [%3]")
                                     .arg(static_cast<char>(mac_src))
                                     .arg(QString::fromStdString(warpped_data.dataframebody().data()))
                                     .arg(QTime::currentTime().toString("hh:mm:ss")));
            try {
                auto &nodeinfo = status_.getNode(mac_src);
            }
            catch (std::runtime_error &e) {
                status_.addNode(NodeInfo(mac_src, client_endpoint.address(), client_endpoint.port(), socket_server_));
            }

            auto &nodeinfo = status_.getNode(mac_src);
            // 设置源节点状态为正在发送
            nodeinfo.setSend(true);
            nodeinfo.setTime(QTime::currentTime());

            emit showStatusSignal(nodeinfo);
            // 查看信道情况

            if (status_.isConflicting(mac_src, mac_conflict)) {  // 信道冲突
                // 设置源节点状态为空闲
                nodeinfo.setSend(false);
                nodeinfo.setTime(QTime());
                emit showStatusSignal(nodeinfo);
                // 广播有冲突发生
                sendConflict(mac_src, mac_conflict);
                emit appendLogSignal(
                    QString("%1 与 %2 发生冲突, 已广播冲突信息. [%3]").arg(static_cast<char>(mac_src)).arg(static_cast<char>(mac_conflict)).arg(QTime::currentTime().toString("hh:mm:ss")));
            }
            else {  // 信道空闲
                // 记下接收时间
                receive_time = QTime::currentTime();

                // 通告有节点在发送
                sendAnnounce(mac_src, receive_time);
                emit appendLogSignal(
                    QString("广播 %1 正在发送, 接收时间 %2. [%3]").arg(static_cast<char>(mac_src)).arg(receive_time.toString("hh:mm:ss")).arg(QTime::currentTime().toString("hh:mm:ss")));
                while (1) {
                    // 等待0.1s
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    // 查看信道情况
                    if (status_.isConflicting(mac_src, mac_conflict)) {  // 信道冲突
                        // 设置源节点状态为空闲

                        nodeinfo.setSend(false);
                        nodeinfo.setTime(QTime());

                        emit showStatusSignal(nodeinfo);
                        // 广播有冲突发生
                        sendConflict(mac_src, mac_conflict);
                        emit appendLogSignal(
                            QString("%1 与 %2 发生冲突, 已广播冲突信息. [%3]").arg(static_cast<char>(mac_src)).arg(static_cast<char>(mac_conflict)).arg(QTime::currentTime().toString("hh:mm:ss")));
                        break;
                    }
                    else {                                                              // 信道空闲
                        if ((int)receive_time.msecsTo(QTime::currentTime()) >= 8000) {  // 已到8s
                            // 向发送者反馈ACK
                            sendACK(mac_src, mac_dest);
                            emit appendLogSignal(QString("已向 %1 发送ACK. [%2]").arg(static_cast<char>(mac_src)).arg(QTime::currentTime().toString("hh:mm:ss")));
                            nodeinfo.setSend(false);
                            nodeinfo.setTime(QTime());
                            emit showStatusSignal(nodeinfo);
                            break;
                        }
                    }
                }
            }
        }
    };

signals:
    void showStatusSignal(const NodeInfo &);
    void appendLogSignal(const QString &);
    void messageBoxSignal(message_func_ptr_t func, QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);

private:
    void showStatus(const NodeInfo &nodeinfo) {
        int           showRow = 0;
        unique_lock_t lock(status_mutex_);

        for (int i = 0; i < window_.ui_->table_mac_status_time->rowCount(); ++i) {
            if (window_.ui_->table_mac_status_time->item(i, 0) == nullptr || window_.ui_->table_mac_status_time->item(i, 0)->text().isEmpty()
                || window_.ui_->table_mac_status_time->item(i, 0)->text()[0] == QString::fromStdString(std::string{ static_cast<char>(nodeinfo.mac_address_) })[0]) {
                showRow = i;
                break;
            }
        }
        window_.ui_->table_mac_status_time->setItem(showRow, 0, new QTableWidgetItem(QString::fromStdString(std::string{ static_cast<char>(nodeinfo.mac_address_) })));
        window_.ui_->table_mac_status_time->setItem(showRow, 1, new QTableWidgetItem(nodeinfo.is_send_ ? "Sending" : "Free"));
        window_.ui_->table_mac_status_time->setItem(showRow, 2, new QTableWidgetItem(nodeinfo.time_.toString("hh:mm:ss")));
    }
    void sendAnnounce(mac_t sender, QTime send_time) {
        uint32_t time = send_time.hour() * 60 * 60 * 1000 + send_time.minute() * 60 * 1000 + send_time.second() * 1000 + send_time.msec();
        for (auto &node : status_.getNodes()) {
            if (node.getMac() != sender) {
                try {
                    DataWarper data;

                    header *header = data.mutable_header();

                    announceFrameBody *body = data.mutable_announceframebody();

                    data.set_type(DataWarper::announceFrame);

                    header->set_destmac(static_cast<google::protobuf::uint32>(node.getMac()));
                    header->set_srcmac(static_cast<google::protobuf::uint32>(sender));
                    body->set_nodesend(static_cast<google::protobuf::uint32>(sender));
                    body->set_sendtime(static_cast<google::protobuf::uint32>(time));

                    size_t      frameLen = data.ByteSizeLong();
                    std::string frame    = data.SerializeAsString();
                    // TODO send header
                    node.socket_.lock()->send(boost::asio::buffer(frame, frameLen));
                }
                catch (boost::system::system_error &err) {
                    emit messageBoxSignal(static_cast<message_func_ptr_t>(QMessageBox::critical), window_.ui_->Widget, "Error", QString::fromStdString(err.what()), QMessageBox::Ok,
                                          QMessageBox::NoButton);
                    return;
                }
            }
        }
    }
    void sendConflict(mac_t sender1, mac_t sender2) {
        for (auto &node : status_.getNodes()) {
            try {
                DataWarper data;

                header *header = data.mutable_header();

                conflictFrameBody *body = data.mutable_conflictframebody();

                data.set_type(DataWarper::conflictFrame);

                header->set_destmac(static_cast<google::protobuf::uint32>(node.getMac()));
                header->set_srcmac(static_cast<google::protobuf::uint32>(sender1));
                body->set_nodesend1(static_cast<google::protobuf::uint32>(sender1));
                body->set_nodesend2(static_cast<google::protobuf::uint32>(sender2));

                size_t      frameLen = data.ByteSizeLong();
                std::string frame    = data.SerializeAsString();

                node.socket_.lock()->send(boost::asio::buffer(frame, frameLen));
            }
            catch (boost::system::system_error &err) {
                emit messageBoxSignal(static_cast<message_func_ptr_t>(QMessageBox::critical), window_.ui_->Widget, "Error", QString::fromStdString(err.what()), QMessageBox::Ok, QMessageBox::NoButton);
                return;
            }
        }
    }
    void sendACK(mac_t mac_src, mac_t mac_dest) {
        DataWarper                data;
        boost::system::error_code ec;

        header *header = data.mutable_header();

        header->set_destmac(static_cast<google::protobuf::uint32>(mac_dest));
        header->set_srcmac(static_cast<google::protobuf::uint32>(mac_src));

        data.set_type(DataWarper::ackFrame);

        ackFrameBody *body = data.mutable_ackframebody();

        socket_server_->send(boost::asio::buffer(data.SerializeAsString()), 0, ec);
        // TODO: 错误处理
        if (ec) {
            emit messageBoxSignal(static_cast<message_func_ptr_t>(QMessageBox::critical), window_.ui_->Widget, "Error", QString::fromStdString(ec.message()), QMessageBox::Ok, QMessageBox::NoButton);
            return;
        }
    }
};

class ListenThread_AP : public QThread {
    Q_OBJECT
public:
    using address_t          = boost::asio::ip::address;
    using port_t             = uint16_t;
    using acceptor_t         = boost::asio::ip::tcp::acceptor;
    using socket_t           = boost::asio::ip::tcp::socket;
    using del_t              = std::function<void(acceptor_t *)>;
    using acceptor_ptr_t     = std::unique_ptr<acceptor_t, del_t>;
    using socket_ptr_t       = std::shared_ptr<socket_t>;
    using endpoint_t         = boost::asio::ip::tcp::endpoint;
    using io_context_t       = boost::asio::io_context;
    using thread_t           = std::thread;
    using thread_ptr_t       = std::unique_ptr<thread_t>;
    using mac_t              = std::byte;
    using message_func_ptr_t = QMessageBox::StandardButton (*)(QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);
    using mutex_t            = std::mutex;
    using shared_mutex_t     = std::shared_mutex;
    using shared_lock_t      = std::shared_lock<shared_mutex_t>;
    using unique_lock_t      = std::unique_lock<shared_mutex_t>;

public:
    explicit ListenThread_AP(Window_AP &window, port_t port, address_t ip, QObject *parent = nullptr) : window_(window), QThread(parent) {
        qRegisterMetaType<message_func_ptr_t>("message_func_ptr_t");
        endpoint_t endpoint(ip, port);
        acceptor_ = acceptor_ptr_t(new acceptor_t(io_context_, endpoint), [](acceptor_t *acceptor) {
            acceptor->close();
            delete acceptor;
        });
        acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_->listen();
        setListenFlag(true);
        QObject::connect(this, &ListenThread_AP::messageBoxSignal, &window_,
                         [this](message_func_ptr_t func, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
                             func(parent, title, text, buttons, defaultButton);
                         });
        QObject::connect(this, &ListenThread_AP::appendLogSignal, &window_, [this](const QString &text) {
            unique_lock_t lock(log_mutex_);
            window_.ui_->textBrowser_log->append(text);
        });
    }
    ~ListenThread_AP() {
        qDebug() << "ListenThread_AP::~ListenThread_AP()";
        for (auto &&[_, thread] : receive_threads_) {
            thread->quit();
            thread->wait();
        }
        if (acceptor_->is_open()) acceptor_->close();
        acceptor_.reset();
        setListenFlag(false);
        qDebug() << "ListenThread_AP::~ListenThread_AP() end";
    }
    void setListenFlag(bool flag) {
        unique_lock_t lock(listen_mutex_);
        listen_flag_ = flag;
    }
    bool getListenFlag() {
        shared_lock_t lock(listen_mutex_);
        return listen_flag_;
    }
    void stop() {
        io_context_.stop();
    }

private:
    io_context_t   io_context_;
    acceptor_ptr_t acceptor_;
    Window_AP     &window_;
    shared_mutex_t status_mutex_;
    shared_mutex_t log_mutex_;
    NodeStatus     status_;

    bool           listen_flag_;
    shared_mutex_t listen_mutex_;

    std::map<endpoint_t, ReceiveThread_AP *> receive_threads_;

protected:
    void run() override {
        qDebug() << "ListenThread_AP::run()";
        using boost::asio::placeholders::error;
        io_context_.reset();
        startAccept();
        io_context_.run();
        qDebug() << "ListenThread_AP::run() end";
    }
    void acceptHandler(const boost::system::error_code &ec, socket_ptr_t socket_server) {
        qDebug() << "ListenThread_AP::acceptHandler()";
        qDebug() << "listen_flag: " << getListenFlag();
        using boost::asio::placeholders::error;
        if (ec || getListenFlag() == false) {
            qDebug() << "ec: " << ec.message() << " listen_flag: " << getListenFlag();
            setListenFlag(false);
            return;
        }
        auto it = receive_threads_.emplace(socket_server->remote_endpoint(), new ReceiveThread_AP(socket_server, log_mutex_, status_mutex_, status_, window_, nullptr));
        QObject::connect(it.first->second, &QThread::finished, it.first->second, &QThread::deleteLater);
        QObject::connect(it.first->second, &QThread::finished, this, [this, it]() { receive_threads_.erase(it.first->first); });
        emit appendLogSignal(QString("与 [%1, %2]建立连接. [%3]")
                                 .arg(socket_server->remote_endpoint().address().to_string().c_str())
                                 .arg(socket_server->remote_endpoint().port())
                                 .arg(QTime::currentTime().toString("hh:mm:ss")));
        it.first->second->start();
        startAccept();
    }
    void startAccept() {
        qDebug() << "ListenThread_AP::startAccept()";
        using boost::asio::placeholders::error;
        socket_ptr_t socket_server = socket_ptr_t(new socket_t(io_context_), [](socket_t *socket) {
            socket->close();
            delete socket;
        });
        acceptor_->async_accept(*socket_server, boost::bind(&ListenThread_AP::acceptHandler, this, error, socket_server));
    }

signals:
    void messageBoxSignal(message_func_ptr_t func, QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);
    void appendLogSignal(const QString &);

private:
};