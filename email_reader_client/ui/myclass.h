#ifndef MYCLASS_H
#define MYCLASS_H

#include "../client.h"
#include <QFile>
#include <QMainWindow>
#include <QTextStream>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QFileSystemWatcher>
#include <QMessageBox>
#include <QString>
#include <cstdlib>

QT_BEGIN_NAMESPACE
namespace Ui {
class MyClass;
}
QT_END_NAMESPACE


class MyClass : public QMainWindow
{
    Q_OBJECT
    QFileSystemWatcher *watcher;
    void loadEmail();
public:
    Client client;
    explicit MyClass(QWidget *parent = nullptr);
    QString readBinaryFileToQString(const QString& filePath);
    ~MyClass();
private slots:
    //void on_pushButton_clicked();
    void onFileChanged(const QString &path);

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

private:
    Ui::MyClass *ui;
};
#endif // MYCLASS_H
