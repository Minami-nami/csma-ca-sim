#include "window_node.h"
#include "ui_Node.h"

Window_Node::Window_Node(QWidget *parent) : QMainWindow(parent), ui_(new Ui::UI_node) {
    ui_->setupUi(this);
}

Window_Node::~Window_Node() {
    delete ui_;
}
