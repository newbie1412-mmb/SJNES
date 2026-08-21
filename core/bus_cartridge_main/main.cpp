#include "SJNES.h"
#include <QtWidgets/QApplication>
#include "SJNES.h"
#include <QtWidgets/QApplication>
#include <QIcon>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    a.setWindowIcon(QIcon("SJNES.ico"));

    SJNES w;
    w.setWindowIcon(QIcon("SJNES.ico"));
    w.show();

    return a.exec();
}