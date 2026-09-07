#pragma once
#include <QString>
#include <QtWidgets/QMainWindow>

namespace Ui
{
	class CheckLogClass;
}

class CheckLog : public QMainWindow
{
	Q_OBJECT

public:
	CheckLog(QWidget* parent = nullptr);
	~CheckLog();

private:
	void ConnectSignalandSlots();

	int fileContainsString(const QString& filePath, const QString& target);
	void InitTableHeader();
	void AddFileResultRow(const QString& file, const QString& result);

private slots:
	void OpenFile();
	void Check();
	void CheckDB();

private:
	Ui::CheckLogClass* _ui = nullptr;
	QString _filePath;
};

