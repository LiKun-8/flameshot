// Copyright 2017 Alejandro Sirgo Rica
//
// This file is part of Flameshot.
//
//     Flameshot is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     Flameshot is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with Flameshot.  If not, see <http://www.gnu.org/licenses/>.

#include "src/core/controller.h"
#include "singleapplication.h"
#include "src/core/flameshotdbusadapter.h"
#include "src/utils/filenamehandler.h"
#include "src/utils/confighandler.h"
#include "src/cli/commandlineparser.h"
#include <QApplication>
#include <QTranslator>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QTextStream>
#include <QDir>

int main(int argc, char *argv[]) {
    // required for the button serialization
    qRegisterMetaTypeStreamOperators<QList<int> >("QList<int>");
    qApp->setApplicationVersion(static_cast<QString>(APP_VERSION));

    QTranslator translator;
    translator.load(QLocale::system().language(),
      "Internationalization", "_", "/usr/share/flameshot/translations/");

    // no arguments, just launch Flameshot
    if (argc == 1) {
        SingleApplication app(argc, argv);
        app.installTranslator(&translator);
        app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings, true);
        app.setApplicationName("flameshot");
        app.setOrganizationName("Dharkael");

        auto c = Controller::getInstance();
        new FlameshotDBusAdapter(c);
        QDBusConnection dbus = QDBusConnection::sessionBus();
        dbus.registerObject("/", c);
        dbus.registerService("org.dharkael.Flameshot");
        return app.exec();
    }

    /*--------------|
     * CLI parsing  |
     * ------------*/
    QCoreApplication app(argc, argv);
    app.setApplicationName("flameshot");
    app.setOrganizationName("Dharkael");
    app.setApplicationVersion(qApp->applicationVersion());
    CommandLineParser parser;
    // Add description
    parser.setDescription(
                "Powerfull yet simple to use screenshot software.");
    parser.setGeneralErrorMessage("See 'flameshot --help'.");
    // Arguments
    CommandArgument fullArgument("full", "Capture the entire desktop.");
    CommandArgument guiArgument("gui", "Start a manual capture in GUI mode.");
    CommandArgument configArgument("config", "Configure flameshot.");

    // Options
    CommandOption pathOption(
                {"p", "path"},
                "Path where the capture will be saved",
                "path");
    CommandOption clipboardOption(
                {"c", "clipboard"},
                "Save the capture to the clipboard");
    CommandOption delayOption(
                {"d", "delay"},
                "Delay time in milliseconds",
                "milliseconds");
    CommandOption filenameOption(
                {"f", "filename"},
                "Set the filename pattern",
                "pattern");
    CommandOption trayOption(
                {"t", "trayicon"},
                "Enable or disable the trayicon",
                "bool");
    CommandOption showHelpOption(
                {"s", "showhelp"},
                "Show the help message in the capture mode",
                "bool");
    CommandOption mainColorOption(
                {"m", "maincolor"},
                "Define the main UI color",
                "color-code");
    CommandOption contrastColorOption(
                {"k", "contrastcolor"},
                "Define the contrast UI color",
                "color-code");

    // Add checkers
    auto colorChecker = [&parser](const QString &colorCode) -> bool {
        QColor parsedColor(colorCode);
        return parsedColor.isValid() && parsedColor.alphaF() == 1.0;
    };
    QString colorErr = "Invalid color, "
                       "this flag supports the following formats:\n"
                       "- #RGB (each of R, G, and B is a single hex digit)\n"
                       "- #RRGGBB\n- #RRRGGGBBB\n"
                       "- #RRRRGGGGBBBB\n"
                       "- Named colors like 'blue' or 'red'\n"
                       "You may need to escape the '#' sign as in '\\#FFF'";

    auto delayChecker = [&parser](const QString &delayValue) -> bool {
        int value = delayValue.toInt();
        return value >= 0;
    };
    QString delayErr = "Ivalid delay, it must be higher than 0";

    auto pathChecker = [&parser](const QString &pathValue) -> bool {
        return QDir(pathValue).exists();
    };
    QString pathErr = "Ivalid path, it must be a real path in the system";

    auto booleanChecker = [&parser](const QString &value) -> bool {
        return value == "true" || value == "false";
    };
    QString booleanErr = "Ivalid value, it must be defined as 'true' or 'false'";

    contrastColorOption.addChecker(colorChecker, colorErr);
    mainColorOption.addChecker(colorChecker, colorErr);
    delayOption.addChecker(delayChecker, delayErr);
    pathOption.addChecker(pathChecker, pathErr);
    trayOption.addChecker(booleanChecker, booleanErr);
    showHelpOption.addChecker(booleanChecker, booleanErr);

    // Relationships
    parser.AddArgument(guiArgument);
    parser.AddArgument(fullArgument);
    parser.AddArgument(configArgument);
    auto helpOption = parser.addHelpOption();
    auto versionOption = parser.addVersionOption();
    parser.AddOptions({ pathOption, delayOption }, guiArgument);
    parser.AddOptions({ pathOption, clipboardOption, delayOption }, fullArgument);
    parser.AddOptions({ filenameOption, trayOption, showHelpOption,
                        mainColorOption, contrastColorOption }, configArgument);
    // Parse
    if (!parser.parse(app.arguments()))
        return 0;

    // PROCESS DATA
    //--------------
    if (parser.isSet(helpOption) || parser.isSet(versionOption)) {
    }
    else if (parser.isSet(guiArgument)) { // GUI
        QString pathValue = parser.value(pathOption);
        int delay = parser.value(delayOption).toInt();

        // Send message
        QDBusMessage m = QDBusMessage::createMethodCall("org.dharkael.Flameshot",
                                           "/", "", "graphicCapture");
        m << pathValue << delay;
        QDBusConnection::sessionBus().call(m);
    }
    else if (parser.isSet(fullArgument)) { // FULL
        QString pathValue = parser.value(pathOption);
        int delay = parser.value(delayOption).toInt();
        bool toClipboard = parser.isSet(clipboardOption);

        // Send message
        QDBusMessage m = QDBusMessage::createMethodCall("org.dharkael.Flameshot",
                                           "/", "", "fullScreen");
        m << pathValue << toClipboard << delay;
        QDBusConnection::sessionBus().call(m);
    }
    else if (parser.isSet(configArgument)) { // CONFIG
        bool filename = parser.isSet(filenameOption);
        bool tray = parser.isSet(trayOption);
        bool help = parser.isSet(showHelpOption);
        bool mainColor = parser.isSet(mainColorOption);
        bool contrastColor = parser.isSet(contrastColorOption);
        bool someFlagSet = (filename || tray || help ||
                            mainColor || contrastColor);
        ConfigHandler config;
        if (filename) {
            QString newFilename(parser.value(filenameOption));
            config.setFilenamePattern(newFilename);
            FileNameHandler fh;
            QTextStream(stdout) << QString("The new pattern is '%1'\n"
                                         "Parsed pattern example: %2\n").arg(newFilename)
                                 .arg(fh.parsedPattern());
        }
        if (tray) {
            QDBusMessage m = QDBusMessage::createMethodCall("org.dharkael.Flameshot",
                                               "/", "", "trayIconEnabled");
            if (parser.value(trayOption) == "false") {
                m << false;
            } else if (parser.value(trayOption) == "true") {
                m << true;
            }
            QDBusConnection::sessionBus().call(m);
        }
        if (help) {
            if (parser.value(showHelpOption) == "false") {
                config.setShowHelp(false);
            } else if (parser.value(showHelpOption) == "true") {
                config.setShowHelp(true);
            }
        }
        if (mainColor) {
            QString colorCode = parser.value(mainColorOption);
            QColor parsedColor(colorCode);
            config.setUIMainColor(parsedColor);
        }
        if (contrastColor) {
            QString colorCode = parser.value(contrastColorOption);
            QColor parsedColor(colorCode);
            config.setUIContrastColor(parsedColor);
        }

        // Open gui when no options
        if (!someFlagSet) {
            QDBusMessage m = QDBusMessage::createMethodCall("org.dharkael.Flameshot",
                                               "/", "", "openConfig");
            QDBusConnection::sessionBus().call(m);
        }
    }
    return 0;
}
