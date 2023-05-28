#include "window_ap.h"
#include "ui_AP.h"

Window_AP::Window_AP(QWidget *parent) : QMainWindow(parent), ui_(new Ui::UI_ap) {
    ui_->setupUi(this);
}

Window_AP::~Window_AP() {
    delete ui_;
}
