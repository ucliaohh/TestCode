#include "CheckLog.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	CheckLog window;
	window.show();
	return app.exec();
}

//Test GitHub: 2026.11.2