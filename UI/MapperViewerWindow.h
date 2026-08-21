#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QFont>

class Bus;

class MapperViewerWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MapperViewerWindow(Bus* bus, QWidget* parent = nullptr);

private slots:
    void updateInfo();

private:
    Bus* bus = nullptr;
    QPlainTextEdit* text = nullptr;
    QTimer* timer = nullptr;
};