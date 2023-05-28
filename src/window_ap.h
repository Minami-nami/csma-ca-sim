#ifndef WINDOW_AP_H
#define WINDOW_AP_H

#include <QMainWindow>

namespace Ui {
class UI_ap;
}

class Window_AP : public QMainWindow {
    Q_OBJECT

public:
    explicit Window_AP(QWidget *parent = 0);
    ~Window_AP();

public:
    Ui::UI_ap *ui_;
};

#endif
