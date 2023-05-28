#include "process_ap.h"

#include "DataWarper.pb.h"

#include "qmessagebox.h"
#include "qobject.h"

#include "qthread.h"
#include "qtmetamacros.h"
#include "qwidget.h"
#include "ui_AP.h"
#include <QMessageBox>
#include <array>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <google/protobuf/stubs/port.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#define DEFAULT_IP_ADDR "127.0.0.1"

Process_AP::Process_AP(int &argc, char **argv) : QApplication(argc, argv), window_() {
    window_.ui_->textBrowser_log->document()->setMaximumBlockCount(100);
    qRegisterMetaType<message_func_ptr_t>("message_func_ptr_t");

    QObject::connect(window_.ui_->button_work, &QPushButton::clicked, this, [this]() {
        int temp_port_ = window_.ui_->text_port->toPlainText().toUInt();
        ip_address_    = address_t::from_string(DEFAULT_IP_ADDR);

        if (listen_thread_ && listen_thread_->getListenFlag() && temp_port_ == port_) {
            QMessageBox::information(&window_, "Info", "Already listening", QMessageBox::Ok);
            return;
        }
        else if (listen_thread_ && listen_thread_->getListenFlag() && temp_port_ != port_) {
            if (listen_thread_->isRunning()) {
                listen_thread_->setListenFlag(false);
                listen_thread_->quit();
                listen_thread_->wait();
            }
            listen_thread_.reset();
            QMessageBox::information(&window_, "Info", "Stop listening", QMessageBox::Ok);
        }
        port_ = temp_port_;

        endpoint_t endpoint(ip_address_, port_);

        listen_thread_ = std::make_unique<ListenThread_AP>(window_, port_, ip_address_, this);
        QObject::connect(listen_thread_.get(), &QThread::finished, this, [this]() { listen_thread_->deleteLater(); });
        listen_thread_->start();
        QMessageBox::information(&window_, "Info", "Start listening", QMessageBox::Ok);
    });

    window_.show();
}

Process_AP::~Process_AP() {
    if (listen_thread_) {
        if (listen_thread_->isRunning()) {
            listen_thread_->stop();
            listen_thread_->setListenFlag(false);
            listen_thread_->quit();
            listen_thread_->wait();
        }
        listen_thread_.reset();
    }
}
