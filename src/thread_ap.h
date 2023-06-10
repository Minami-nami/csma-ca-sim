#pragma once
#include "DataWarper.pb.h"
#include "NodeInfo.h"
#include "NodeStatus.h"
#include "qdatetime.h"
#include "qmessagebox.h"
#include "qobject.h"
#include "qthread.h"
#include "qtmetamacros.h"
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
#include <semaphore>
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
    explicit ReceiveThread_AP(socket_ptr_t socket_server, shared_mutex_t &log_mutex, shared_mutex_t &status_mutex, NodeStatus &status, Window_AP &window, QObject *parent = nullptr);
    ~ReceiveThread_AP();

private:
    socket_ptr_t    socket_server_;
    shared_mutex_t &log_mutex_;
    shared_mutex_t &status_mutex_;
    NodeStatus     &status_;
    Window_AP      &window_;
    mac_t           mac_current_ = mac_t{ 0 };

protected:
    void run() override;

signals:
    void showStatusSignal(const NodeInfo &);
    void appendLogSignal(const QString &);
    void messageBoxSignal(message_func_ptr_t func, QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);

private:
    void showStatus(const NodeInfo &nodeinfo);
    void sendAnnounce(mac_t sender, QTime send_time);
    void sendConflict(mac_t sender1, mac_t sender2);
    void sendACK(mac_t mac_src, mac_t mac_dest);
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
    explicit ListenThread_AP(Window_AP &window, port_t port, address_t ip, QObject *parent = nullptr);
    ~ListenThread_AP();
    void setListenFlag(bool flag);
    bool getListenFlag();
    void stop();

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
    void run() override;
    void acceptHandler(const boost::system::error_code &ec, socket_ptr_t socket_server);
    void startAccept();

signals:
    void messageBoxSignal(message_func_ptr_t func, QWidget *, const QString &, const QString &, QMessageBox::StandardButtons, QMessageBox::StandardButton);
    void appendLogSignal(const QString &);

private:
};