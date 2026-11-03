#include "CheckLog.h"
#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	CheckLog window;
	window.show();
	return app.exec();
}

//Test Pr-01