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