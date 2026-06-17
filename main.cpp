#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_AttributeCount);
    QApplication app(argc, argv);

    MainWindow  window;
    window.show();

    return app.exec();
}
