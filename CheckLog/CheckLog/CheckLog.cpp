#include "CheckLog.h"
#include "ui_CheckLog.h"
#include "sqlite3.h"

#include <algorithm>
#include <cstring>
#include <thread>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QPointer>
#include <QTextStream>

#pragma comment(lib, "sqlite3.lib")

namespace
{
	const char* kDbPassword = "AceASyS";

	QString QueryAssayResult(const QString& dbPath, const QString& expId)
	{
		sqlite3* db = nullptr;
		int openResult = sqlite3_open16(dbPath.utf16(), &db);
		if (openResult != SQLITE_OK)
		{
			QString errorText = db != nullptr ? QString::fromUtf8(sqlite3_errmsg(db)) : QStringLiteral("unknown error");
			if (db != nullptr)
				sqlite3_close(db);
			return QStringLiteral("打开失败: %1").arg(errorText);
		}

		int keyResult = sqlite3_key(db, kDbPassword, static_cast<int>(strlen(kDbPassword)));
		if (keyResult != SQLITE_OK)
		{
			QString errorText = QString::fromUtf8(sqlite3_errmsg(db));
			sqlite3_close(db);
			return QStringLiteral("设置密码失败: %1").arg(errorText);
		}

		const char* sql = "SELECT 1 FROM aAssays WHERE ExpID = ? LIMIT 1;";
		sqlite3_stmt* stmt = nullptr;
		int prepareResult = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
		if (prepareResult != SQLITE_OK)
		{
			QString errorText = QString::fromUtf8(sqlite3_errmsg(db));
			sqlite3_close(db);
			return QStringLiteral("查询失败: %1").arg(errorText);
		}

		sqlite3_bind_text16(stmt, 1, expId.utf16(), -1, SQLITE_TRANSIENT);

		QString result;
		int stepResult = sqlite3_step(stmt);
		if (stepResult == SQLITE_ROW)
			result = QStringLiteral("存在");
		else if (stepResult == SQLITE_DONE)
			result = QStringLiteral("不存在");
		else
			result = QStringLiteral("查询失败: %1").arg(QString::fromUtf8(sqlite3_errmsg(db)));

		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return result;
	}
}

CheckLog::CheckLog(QWidget* parent)
	: QMainWindow(parent)
	, _ui(new Ui::CheckLogClass)
{
	_ui->setupUi(this);

	ConnectSignalandSlots();

	_filePath = QCoreApplication::applicationDirPath();
	_ui->lineEdit->setText(_filePath);

	InitTableHeader();
}

CheckLog::~CheckLog()
{
	delete _ui;
}

void CheckLog::ConnectSignalandSlots()
{
	connect(_ui->btn_Open, SIGNAL(clicked()), this, SLOT(OpenFile()));
	connect(_ui->btn_Check, SIGNAL(clicked()), this, SLOT(Check()));

	connect(_ui->btn_CheckDB, SIGNAL(clicked()), this, SLOT(CheckDB()));
}

void CheckLog::OpenFile()
{
	QString filepath = QFileDialog::getExistingDirectory(this, "", _filePath);

	if (filepath == "")
		_ui->lineEdit->setText(_filePath);
	else
		_ui->lineEdit->setText(filepath);

	_filePath = _ui->lineEdit->text();
	_ui->tableWidget->setRowCount(0);
}

int CheckLog::fileContainsString(const QString& filePath, const QString& target)
{
	int count = 0;

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return count;
	QTextStream in(&file);
	while (!in.atEnd())
	{
		QString line = in.readLine();
		if (line.contains(target, Qt::CaseSensitive))
			count++;
	}

	return count;
}

void CheckLog::InitTableHeader()
{
	QStringList headers;
	headers << "File" << "Result";
	_ui->tableWidget->setColumnCount(headers.size());
	_ui->tableWidget->setHorizontalHeaderLabels(headers);

	_ui->tableWidget->setColumnWidth(0, 580);
	_ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

	_ui->tableWidget->setShowGrid(true);
}

void CheckLog::AddFileResultRow(const QString& file, const QString& result)
{
	int row = _ui->tableWidget->rowCount();
	_ui->tableWidget->insertRow(row);

	{
		QTableWidgetItem* item = new QTableWidgetItem(file);
		item->setTextAlignment(Qt::AlignCenter);
		_ui->tableWidget->setItem(row, 0, item);
	}

	{
		QTableWidgetItem* item = new QTableWidgetItem(result);
		item->setTextAlignment(Qt::AlignCenter);
		if (result == QStringLiteral("存在"))
			item->setForeground(QBrush(QColor(Qt::green)));
		_ui->tableWidget->setItem(row, 1, item);
	}
}

void CheckLog::Check()
{
	_ui->tableWidget->setRowCount(0);

	QString rootDir = _ui->lineEdit->text();
	QString target = "Camera error retry time:";

	QList<QString> items;
	QDirIterator it(rootDir, QStringList() << "*CameraLog.txt", QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		QString filePath = it.next();
		items.push_back(filePath);
	}

	// Sort by name length (shorter names first)
	std::sort(items.begin(), items.end(), [](const QString& a, const QString& b) {
		return a.length() < b.length();
		});

	for (auto file : items)
	{
		int count = fileContainsString(file, target);
		AddFileResultRow(file, QString::number(count));
	}
}

void CheckLog::CheckDB()
{
	QString expId = _ui->lineEdit_ExpID->text().trimmed();
	if (expId.isEmpty())
	{
		statusBar()->showMessage(QStringLiteral("请输入 ExpID"));
		return;
	}

	_ui->tableWidget->setRowCount(0);
	_ui->btn_CheckDB->setEnabled(false);
	statusBar()->showMessage(QStringLiteral("正在扫描 AceaDB 文件并查询..."));

	QPointer<CheckLog> that(this);
	std::thread([that, expId]() {
		QList<QString> dbFiles;
		const QFileInfoList drives = QDir::drives();
		for (const QFileInfo& drive : drives)
		{
			QDirIterator it(drive.absoluteFilePath(), QStringList() << "*AceaDB*.sys", QDir::Files | QDir::Readable | QDir::NoSymLinks, QDirIterator::Subdirectories);
			while (it.hasNext())
				dbFiles.push_back(it.next());
		}

		std::sort(dbFiles.begin(), dbFiles.end(), [](const QString& a, const QString& b) {
			return a.length() < b.length();
			});

		for (const QString& filePath : dbFiles)
		{
			QString result = QueryAssayResult(filePath, expId);
			if (that)
			{
				QMetaObject::invokeMethod(that, [that, filePath, result]() {
					if (!that)
						return;
					that->AddFileResultRow(filePath, result);
					}, Qt::QueuedConnection);
			}
		}

		if (that)
		{
			QMetaObject::invokeMethod(that, [that, dbFiles]() {
				if (!that)
					return;
				that->_ui->btn_CheckDB->setEnabled(true);
				that->statusBar()->showMessage(QStringLiteral("查询完成，共检查 %1 个 AceaDB 文件").arg(dbFiles.size()));
				}, Qt::QueuedConnection);
		}
		}).detach();
}
