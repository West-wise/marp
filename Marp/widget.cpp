#include "widget.h"
#include "./ui_widget.h"

Widget::Widget(const QString file1, const QString file2, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
    , interface_list(file2)
    , arp_file(file1)
    , destinationDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/")
{
    ui->setupUi(this);
    packet_mode = true;
    append_interface_list();

    //destinationDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/";
    // 파일 복사
    if (!copyFileFromAssets(file1, destinationDir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
        qWarning() << "Failed to copy" << file1 << "to" << destinationDir;
    }
    if (!copyFileFromAssets(file2, destinationDir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) {
        qWarning() << "Failed to copy" << file2 << "to" << destinationDir;
    }


    //ui->btn_stop->setDisabled(true);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::press_start()
{
    ui->btn_send->setDisabled(true);
    ui->btn_stop->setEnabled(true);
}

void Widget::press_stop()
{
    ui->btn_send->setEnabled(true);
    ui->btn_stop->setDisabled(true);
}

bool Widget::copyFileFromAssets(QString fileName, QString dir, QFile::Permissions permissions) {

    QString srcFileName = ":/assets/" + fileName;
    QString dstFileName = dir + fileName;

    QFile srcFile(srcFileName);
    if (!srcFile.exists()) {
        qWarning() << "Source file does not exist:" << srcFileName;
        return false;
    }
    QFile dstFile(dstFileName);
    if (dstFile.exists()) {
        if (!dstFile.remove()) {
            qWarning() << "Failed to remove existing file:" << dstFileName;
            return false;
        }
    }
    if (!srcFile.copy(dstFileName)) {
        qWarning() << "Failed to copy file from" << srcFileName << "to" << dstFileName;
        return false;
    }
    if (!dstFile.setPermissions(permissions)) {
        qWarning() << "Failed to set permissions on:" << dstFileName;
        return false;
    }
    return true;
}


bool Widget::check_address_text_box()
{
    QString targetIpText = ui->txt_target->text();
    QString senderIpText = ui->txt_sender->text();

    if (targetIpText.isEmpty() || senderIpText.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please fill in both IP addresses.");
        press_stop();
        return false;
    }

    return true;
}

void Widget::on_rb_broadcast_clicked()
{
    packet_mode = false;
    ui->txt_sender->setDisabled(true);
}


void Widget::on_rb_unicast_clicked()
{
    packet_mode = true;
    ui->txt_sender->setEnabled(true);

}

void Widget::append_interface_list()
{
    QString interface_list_FilePath = destinationDir + interface_list;
    QProcess interface_list_process;
    QFile file(interface_list_FilePath);

    if (!file.exists()) {
        qDebug() << "File does not exist at: " << interface_list_FilePath;
        ui->logbox->appendPlainText("File does not exist");
        return;
    }

    interface_list_process.start("su", QStringList() << "-c" << interface_list_FilePath);
    if (!interface_list_process.waitForStarted(1000)) {
        ui->logbox->appendPlainText("Failed to start process");
        return;
    } else {
        ui->logbox->appendPlainText("Process started successfully");
    }

    interface_list_process.waitForFinished(3000);
    QString output = interface_list_process.readAllStandardOutput();
    QString errorOutput = interface_list_process.readAllStandardError();

    if (!output.isEmpty()) {
        ui->logbox->appendPlainText("Standard Output: " + output);
    }

    if (!errorOutput.isEmpty()) {
        ui->logbox->appendPlainText("Error Output: " + errorOutput);
    }

    // QComboBox 초기화
    ui->cb_iflist->clear();

    // 파일에서 읽어들인 결과를 QComboBox에 추가
    QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty()) {
            QStringList parts = line.split(':');
            if (parts.size() == 2) {
                QString interface_name = parts[1].trimmed();
                ui->cb_iflist->addItem(interface_name);
            }
        }
    }

    dev = lines[0].split(':')[1];
    qDebug() << "dev : " << dev;
}


void Widget::on_cb_iflist_activated(int index)
{
    dev = ui->cb_iflist->currentText().trimmed();
    ui->logbox->appendPlainText(dev);
}

void Widget::on_txt_sender_editingFinished()
{
    sender_ip = ui->txt_sender->text().trimmed();
}

void Widget::on_txt_target_editingFinished()
{
    target_ip = ui->txt_target->text().trimmed();
}

void Widget::killProcess(QProcess *process)
{
    if (process->state() == QProcess::Running) {
        process->terminate();
        if (!process->waitForFinished(5000)) {
            process->kill();
        }
        qDebug() << "Process terminated.";
        ui->logbox->appendPlainText("Process terminated.");
    } else {
        qDebug() << "No process running.";
        ui->logbox->appendPlainText("No process running.");
    }
}

void Widget::killAllProcess()
{
    for (QProcess *process : qAsConst(processList)) {
        killProcess(process);
    }
}

void Widget::on_btn_send_clicked()
{
    QString objectFilePath = destinationDir + arp_file;
    QStringList address_args;
    QFile file(objectFilePath);

    if (!file.exists()) {
        qDebug() << "File does not exist at: " << objectFilePath;
        return;
    }
    if (check_address_text_box()){
        press_start();
        if(packet_mode){
            address_args << dev << sender_ip << target_ip;
        } else {
            address_args << dev << target_ip;
        }

        QProcess *arp_process = new QProcess(this);
        connect(arp_process, &QProcess::errorOccurred, this, &Widget::handleProcessError);
        //connect(ui->btn_stop, &QPushButton::clicked, this, &Widget::on_btn_stop_clicked);

        QStringList arguments;
        arguments << "-c" << objectFilePath + " " + address_args.join(" ");
        arp_process->start("su", arguments);
        if (!arp_process->waitForStarted(5000)) {
            qDebug() << "Failed to start process";
            ui->logbox->appendPlainText("Failed to start process");
            return;
        } else {
            qDebug() << "Process started successfully.";
            qDebug() << arp_process->arguments();
            ui->logbox->appendPlainText("Process started successfully");
        }
        processList.append(arp_process);

    }
}

void Widget::handleProcessError(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        qDebug() << "The process failed to start.";
        ui->logbox->appendPlainText("The process failed to start.");
        break;
    case QProcess::Crashed:
        qDebug() << "The process crashed.";
        ui->logbox->appendPlainText("The process crashed.");
        break;
    case QProcess::Timedout:
        qDebug() << "The process timed out.";
        ui->logbox->appendPlainText("The process timed out.");
        break;
    case QProcess::WriteError:
        qDebug() << "An error occurred when attempting to write to the process.";
        ui->logbox->appendPlainText("An error occurred when attempting to write to the process.");
        break;
    case QProcess::ReadError:
        qDebug() << "An error occurred when attempting to read from the process.";
        ui->logbox->appendPlainText("An error occurred when attempting to read from the process.");
        break;
    default:
        qDebug() << "An unknown error occurred.";
        ui->logbox->appendPlainText("An unknown error occurred.");
    }
}


void Widget::on_btn_stop_clicked()
{
    ui->btn_send->setEnabled(true);
    ui->btn_stop->setDisabled(true);
    killAllProcess();
}


void Widget::on_btn_clearlog_clicked()
{
    ui->logbox->clear();
}

