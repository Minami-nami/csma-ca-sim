#pragma once
#include "qmessagebox.h"
#include "qtmetamacros.h"
#include "qwidget.h"
#include "thread_ap.h"
#include "window_ap.h"
#include <QApplication>
#include <boost/asio.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

class Process_AP : public QApplication {
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

private:
    port_t                           port_;
    address_t                        ip_address_;
    Window_AP                        window_;
    std::unique_ptr<ListenThread_AP> listen_thread_;

public:
    Process_AP(int &argc, char **argv);
    ~Process_AP();
};