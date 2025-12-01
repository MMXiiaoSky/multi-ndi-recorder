#include <QApplication>
#include "MainWindow.h"
#include "Logging.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Logger::instance().log("Application started");
    MainWindow w;
    w.show();
    int ret = a.exec();
    Logger::instance().log("Application exit");
    return ret;
}
