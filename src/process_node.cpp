#include "process_node.h"
#include "DataWarper.pb.h"
#include "qdebug.h"
#include "qmessagebox.h"
#include "qobject.h"
#include "qpushbutton.h"
#include "ui_Node.h"
#include <QDebug>
#include <QMessageBox>
#include <array>
#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

Process_Node::Process_Node(int &argc, char **argv) : QApplication(argc, argv), window_(), opinion_(true) {
    connected_ = false;
    status_    = status::free;
    window_.ui_->text_Port_1->setReadOnly(true);
    thread_receive_ = nullptr;
    thread_send_    = nullptr;
    QObject::connect(window_.ui_->button_Send, &QPushButton::clicked, this, [this]() {
        if (thread_send_) {
            QMessageBox::warning(&window_, "Warning", "current node is sending", QMessageBox::Ok, QMessageBox::NoButton);
            return;
        }
        if (!socket_client_ || !socket_client_->is_open() || !connected_) {
            QMessageBox::warning(&window_, "Warning", "current node is not connected", QMessageBox::Ok, QMessageBox::NoButton);
            return;
        }
        thread_send_ = new ThreadSend_Node(window_, *this, socket_client_, mac_current_, this);
        QObject::connect(thread_send_, &ThreadSend_Node::finished, thread_send_, &ThreadSend_Node::deleteLater);
        QObject::connect(thread_send_, &ThreadSend_Node::taskend, this, [this]() { thread_send_ = nullptr; });
        thread_send_->setSending(true);
        thread_send_->start();
    });
    QObject::connect(window_.ui_->button_Connect, &QPushButton::clicked, this, [this]() {
        if (socket_client_ && socket_client_->is_open() && connected_) {
            QMessageBox::warning(&window_, "Warning", "current node is already connected", QMessageBox::Ok, QMessageBox::NoButton);
            return;
        }
        mac_current_ = static_cast<std::byte>(window_.ui_->comboBox_MAC->currentText().toStdString()[0]);
        try {
            address_current_ = boost::asio::ip::address::from_string(window_.ui_->text_IP_Cur->toPlainText().toStdString());
            address_server_  = boost::asio::ip::address::from_string(window_.ui_->text_IP_AP->toPlainText().toStdString());
        }
        catch (boost::system::system_error &ec) {
            QMessageBox::warning(&window_, "Warning", ec.what(), QMessageBox::Ok, QMessageBox::NoButton);
            return;
        }
        port_current_ = static_cast<unsigned short>(window_.ui_->text_Port_1->toPlainText().toUInt());
        port_server_  = static_cast<unsigned short>(window_.ui_->text_Port_2->toPlainText().toUInt());

        socket_client_ = socket_ptr_t(new socket_t(io_context_), [](socket_t *socket) {
            socket->close();
            delete socket;
        });
        boost::system::error_code ec;
        socket_client_->connect(endpoint_t(address_server_, port_server_), ec);
        if (ec) {
            QMessageBox::warning(&window_, "Warning", ec.message().c_str(), QMessageBox::Ok, QMessageBox::NoButton);
            socket_client_.reset();
            return;
        }
        window_.ui_->text_Port_1->setPlainText(QString::number(socket_client_->local_endpoint().port()));
        connected_      = true;
        thread_receive_ = new ThreadReceive_Node(*this, window_, socket_client_, io_context_, mac_current_, this);
        QObject::connect(thread_receive_, &ThreadReceive_Node::finished, thread_receive_, &ThreadReceive_Node::deleteLater);
        QObject::connect(thread_receive_, &ThreadReceive_Node::taskend, this, [this]() {
            thread_receive_ = nullptr;
            connected_      = false;
        });
        thread_receive_->start();
        QMessageBox::information(&window_, "Info", "Connect success", QMessageBox::Ok, QMessageBox::NoButton);
    });
    window_.show();
}

Process_Node::~Process_Node() {
    try {
        io_context_.stop();
        socket_client_->close();
        socket_client_.reset();
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
}

status Process_Node::getstatus() {
    shared_lock_t lock(status_lock_);
    return status_;
}
void Process_Node::setstatus(status status) {
    unique_lock_t lock(status_lock_);
    status_ = status;
}
QTime Process_Node::gettime() {
    shared_lock_t lock(time_lock_);
    return transform_time_;
}
void Process_Node::settime(QTime time) {
    unique_lock_t lock(time_lock_);
    transform_time_ = time;
}
bool Process_Node::isQueueEmpty() {
    shared_lock_t lock(queue_lock_);
    return queue_.empty();
}
void Process_Node::pushQueue(const DataWarper &data) {
    unique_lock_t lock(queue_lock_);
    queue_.push(data);
}
void Process_Node::popQueue() {
    unique_lock_t lock(queue_lock_);
    queue_.pop();
}
void Process_Node::cleanQueue() {
    unique_lock_t lock(queue_lock_);
    queue_t       empty;
    queue_.swap(empty);
}
const DataWarper &Process_Node::frontQueue() {
    shared_lock_t lock(queue_lock_);
    return queue_.front();
}

void ThreadReceive_Node::run() {
    qDebug() << "starting receive";
    using boost::asio::placeholders::error;
    io_context_.reset();
    socket_client_->async_receive(boost::asio::buffer(buffer), boost::bind(&ThreadReceive_Node::receiveHandler, this, error, boost::asio::placeholders::bytes_transferred));
    io_context_.run();
    qDebug() << "ending receive";
    quit();
}

ThreadReceive_Node::ThreadReceive_Node(Process_Node &node, Window_Node &window, socket_ptr_t socket_client, io_context_t &io_context, mac_t &mac_current, QObject *parent)
    : app_(node), window_(window), socket_client_(socket_client), io_context_(io_context), mac_current_(mac_current), QThread(parent) {
    qRegisterMetaType<message_func_ptr_t>("message_func_ptr_t");
    io_context_.reset();
    QObject::connect(this, &ThreadReceive_Node::messageBoxSignal, &app_,
                     [this](message_func_ptr_t func, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons button, QMessageBox::StandardButton nobutton) {
                         func(parent, title, text, button, nobutton);
                     });
    QObject::connect(this, &ThreadReceive_Node::setText, &app_, [this](const QString &str) { window_.ui_->text_Receive->setText(str); });
}

ThreadReceive_Node::~ThreadReceive_Node() {
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

void ThreadReceive_Node::receiveHandler(const boost::system::error_code &ec, size_t bytes_transferred) {
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

void ThreadSend_Node::run() {
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

ThreadSend_Node::ThreadSend_Node(Window_Node &window, Process_Node &node, socket_ptr_t socket_client, mac_t mac_current, QObject *parent)
    : backoff_time_(0), backoff_counter_(0), window_(window), sending_(false), app_(node), socket_client_(socket_client), mac_current_(mac_current), QThread(parent) {
    qRegisterMetaType<message_func_ptr_t>("message_func_ptr_t");
    QObject::connect(this, &ThreadSend_Node::messageBoxSignal, &app_,
                     [this](message_func_ptr_t func, QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons button, QMessageBox::StandardButton nobutton) {
                         func(parent, title, text, button, nobutton);
                     });
    QObject::connect(this, &ThreadSend_Node::setText, &app_, [this](const QString &str) { window_.ui_->text_Receive->setText(str); });
}

ThreadSend_Node::~ThreadSend_Node() {
    wait();
    qDebug() << "deconstructing send";
    socket_client_.reset();
    qDebug() << "deconstructed send";
    emit taskend();
}
bool ThreadSend_Node::getSending() {
    std::lock_guard<mutex_t> lock(sending_lock_);
    return sending_;
}
void ThreadSend_Node::setSending(bool sending) {
    std::lock_guard<mutex_t> lock(sending_lock_);
    sending_ = sending;
}