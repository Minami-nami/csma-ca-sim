#include "process_node.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    Process_Node app(argc, argv);
    return app.exec();
}
