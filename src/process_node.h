#pragma once
#include "DataWarper.pb.h"
#include "qtmetamacros.h"
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

    status getstatus();
    void   setstatus(status status);
    QTime  gettime();
    void   settime(QTime time);
    bool   isQueueEmpty();
    void   pushQueue(const DataWarper &data);
    void   popQueue();
    void   cleanQueue();

    const DataWarper &frontQueue();
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
    void run() override;

public:
    ThreadReceive_Node(Process_Node &node, Window_Node &window, socket_ptr_t socket_client, io_context_t &io_context, mac_t &mac_current, QObject *parent = nullptr);
    ~ThreadReceive_Node();

private:
    void receiveHandler(const boost::system::error_code &ec, size_t bytes_transferred);

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
    void run() override;

public:
    ThreadSend_Node(Window_Node &window, Process_Node &node, socket_ptr_t socket_client, mac_t mac_current, QObject *parent = nullptr);
    ~ThreadSend_Node();
    bool getSending();
    void setSending(bool sending);

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