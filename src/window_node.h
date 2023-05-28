#ifndef WINDOW_NODE_H
#define WINDOW_NODE_H

#include <QMainWindow>

namespace Ui {
class UI_node;
}

class Window_Node : public QMainWindow {
    Q_OBJECT

public:
    explicit Window_Node(QWidget *parent = 0);
    ~Window_Node();

public:
    Ui::UI_node *ui_;
};

#endif
