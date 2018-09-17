// This file is part of Heimer.
// Copyright (C) 2018 Jussi Lind <jussi.lind@iki.fi>
//
// Heimer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// Heimer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Heimer. If not, see <http://www.gnu.org/licenses/>.

#include "application.hpp"
#include "config.hpp"
#include "editordata.hpp"
#include "editorscene.hpp"
#include "editorview.hpp"
#include "exporttopngdialog.hpp"
#include "mainwindow.hpp"
#include "mediator.hpp"
#include "statemachine.hpp"
#include "userexception.hpp"

#include "contrib/mclogger.hh"

#include <QColorDialog>
#include <QFileDialog>
#include <QLocale>
#include <QMessageBox>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>

#include <iostream>

namespace {

static const QString FILE_EXTENSION(Config::FILE_EXTENSION);

static void printHelp()
{
    std::cout << std::endl << "Heimer version " << VERSION << std::endl;
    std::cout << Config::COPYRIGHT << std::endl << std::endl;
    std::cout << "Usage: heimer [options] [mindMapFile]" << std::endl << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "--help        Show this help." << std::endl;
    std::cout << "--lang [lang] Force language: fi." << std::endl;
    std::cout << std::endl;
}

static void initTranslations(QTranslator & appTranslator, QGuiApplication & app, QString lang = "")
{
    if (lang == "")
    {
        lang = QLocale::system().name();
    }

    if (appTranslator.load(Config::TRANSLATIONS_RESOURCE_BASE + lang))
    {
        app.installTranslator(&appTranslator);
        MCLogger().info() << "Loaded translations for " << lang.toStdString();
    }
    else
    {
        MCLogger().warning() << "Failed to load translations for " << lang.toStdString();
    }
}

}

void Application::parseArgs(int argc, char ** argv)
{
    QString lang = "";

    const std::vector<QString> args(argv, argv + argc);
    for (unsigned int i = 1; i < args.size(); i++)
    {
        if (args[i] == "-h" || args[i] == "--help")
        {
            printHelp();
            throw UserException("Exit due to help.");
        }
        else if (args[i] == "--lang" && (i + i) < args.size())
        {
            lang = args[i + 1];
            i++;
        }
        else
        {
            m_mindMapFile = args[i];
        }
    }

    initTranslations(m_appTranslator, m_app, lang);
}

Application::Application(int & argc, char ** argv)
    : m_app(argc, argv)
    , m_stateMachine(new StateMachine)
    , m_mainWindow(new MainWindow)
    , m_mediator(new Mediator(*m_mainWindow))
    , m_editorData(new EditorData)
    , m_editorScene(new EditorScene)
    , m_editorView(new EditorView(*m_mediator))
    , m_exportToPNGDialog(new ExportToPNGDialog(m_mainWindow.get()))
{
    parseArgs(argc, argv);

    m_mainWindow->setMediator(m_mediator);
    m_stateMachine->setMediator(m_mediator);

    m_mediator->setEditorData(m_editorData);
    m_mediator->setEditorScene(m_editorScene);
    m_mediator->setEditorView(*m_editorView);

    // Connect views and StateMachine together
    connect(this, &Application::actionTriggered, m_stateMachine.get(), &StateMachine::calculateState);
    connect(m_editorView, &EditorView::actionTriggered, m_stateMachine.get(), &StateMachine::calculateState);
    connect(m_mainWindow.get(), &MainWindow::actionTriggered, m_stateMachine.get(), &StateMachine::calculateState);
    connect(m_stateMachine.get(), &StateMachine::stateChanged, this, &Application::runState);

    connect(m_editorData.get(), &EditorData::isModifiedChanged, [=] (bool isModified) {
        m_mainWindow->enableSave(isModified && m_mediator->canBeSaved());
    });

    connect(m_exportToPNGDialog.get(), &ExportToPNGDialog::pngExportRequested, m_mediator.get(), &Mediator::exportToPNG);

    connect(m_mediator.get(), &Mediator::exportFinished, m_exportToPNGDialog.get(), &ExportToPNGDialog::finishExport);

    m_mainWindow->initialize();
    m_mediator->initializeView();

    m_mainWindow->show();

    if (!m_mindMapFile.isEmpty())
    {
        QTimer::singleShot(0, this, &Application::openArgMindMap);
    }
}

QString Application::getFileDialogFileText() const
{
    return tr("Heimer Files") + " (*" + FILE_EXTENSION + ")";
}

QString Application::loadRecentPath() const
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    const auto path = settings.value("recentPath", QStandardPaths::writableLocation(QStandardPaths::HomeLocation)).toString();
    settings.endGroup();
    return path;
}

int Application::run()
{
    return m_app.exec();
}

void Application::runState(StateMachine::State state)
{
    switch (state)
    {
    case StateMachine::State::TryCloseWindow:
        m_mainWindow->saveWindowSize();
        m_mainWindow->close();
        break;
    case StateMachine::State::Exit:
        m_mainWindow->saveWindowSize();
        QApplication::exit(EXIT_SUCCESS);
        break;
    default:
    case StateMachine::State::Edit:
        m_mainWindow->setTitle();
        break;
    case StateMachine::State::InitializeNewMindMap:
        m_mediator->initializeNewMindMap();
        break;
    case StateMachine::State::SaveMindMap:
        saveMindMap();
        break;
    case StateMachine::State::ShowBackgroundColorDialog:
        showBackgroundColorDialog();
        break;
    case StateMachine::State::ShowExportToPNGDialog:
        showExportToPNGDialog();
        break;
    case StateMachine::State::ShowNotSavedDialog:
        switch (showNotSavedDialog())
        {
        case QMessageBox::Save:
            emit actionTriggered(StateMachine::Action::NotSavedDialogAccepted);
            break;
        case QMessageBox::Discard:
            emit actionTriggered(StateMachine::Action::NotSavedDialogDiscarded);
            break;
        case QMessageBox::Cancel:
            emit actionTriggered(StateMachine::Action::NotSavedDialogCanceled);
            break;
        }
        break;
    case StateMachine::State::ShowSaveAsDialog:
        saveMindMapAs();
        break;
    case StateMachine::State::ShowOpenDialog:
        openMindMap();
        break;
    }
}

void Application::openArgMindMap()
{
    doOpenMindMap(m_mindMapFile);
}

void Application::openMindMap()
{
    MCLogger().debug() << "Open file";

    const auto path = loadRecentPath();
    const auto fileName = QFileDialog::getOpenFileName(m_mainWindow.get(), tr("Open File"), path, getFileDialogFileText());
    if (!fileName.isEmpty())
    {
        doOpenMindMap(fileName);
    }
}

void Application::doOpenMindMap(QString fileName)
{
    MCLogger().debug() << "Opening '" << fileName.toStdString();

    if (m_mediator->openMindMap(fileName))
    {
        m_mainWindow->disableUndoAndRedo();

        saveRecentPath(fileName);

        m_mainWindow->setSaveActionStatesOnOpenedMindMap();

        emit actionTriggered(StateMachine::Action::MindMapOpened);
    }
}

void Application::saveMindMap()
{
    MCLogger().debug() << "Save..";

    if (!m_mediator->saveMindMap())
    {
        const auto msg = QString(tr("Failed to save file."));
        MCLogger().error() << msg.toStdString();
        showMessageBox(msg);
        emit actionTriggered(StateMachine::Action::MindMapSaveFailed);
        return;
    }

    m_mainWindow->enableSave(false);
    emit actionTriggered(StateMachine::Action::MindMapSaved);
}


void Application::saveMindMapAs()
{
    MCLogger().debug() << "Save as..";

    QString fileName = QFileDialog::getSaveFileName(
        m_mainWindow.get(),
        tr("Save File As"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        getFileDialogFileText());

    if (fileName.isEmpty())
    {
        return;
    }

    if (!fileName.endsWith(FILE_EXTENSION))
    {
        fileName += FILE_EXTENSION;
    }

    if (m_mediator->saveMindMapAs(fileName))
    {
        const auto msg = QString(tr("File '")) + fileName + tr("' saved.");
        MCLogger().debug() << msg.toStdString();
        emit actionTriggered(StateMachine::Action::MindMapSavedAs);
    }
    else
    {
        const auto msg = QString(tr("Failed to save file as '") + fileName + "'.");
        MCLogger().error() << msg.toStdString();
        showMessageBox(msg);
        emit actionTriggered(StateMachine::Action::MindMapSaveAsFailed);
    }
}

void Application::saveRecentPath(QString fileName)
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue("recentPath", fileName);
    settings.endGroup();
}

void Application::showBackgroundColorDialog()
{
    const auto color = QColorDialog::getColor(Qt::white, m_mainWindow.get());
    if (color.isValid()) {
        m_mediator->setBackgroundColor(color);
    }
    emit actionTriggered(StateMachine::Action::BackgroundColorChanged);
}

void Application::showExportToPNGDialog()
{
    m_exportToPNGDialog->setImageSize(m_mediator->zoomForExport());
    m_exportToPNGDialog->exec();

    // Doesn't matter if canceled or not
    emit actionTriggered(StateMachine::Action::ExportedToPNG);
}

void Application::showMessageBox(QString message)
{
    QMessageBox msgBox(m_mainWindow.get());
    msgBox.setText(message);
    msgBox.exec();
}

int Application::showNotSavedDialog()
{
    QMessageBox msgBox(m_mainWindow.get());
    msgBox.setText(tr("The mind map has been modified."));
    msgBox.setInformativeText(tr("Do you want to save your changes?"));
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);
    return msgBox.exec();
}

Application::~Application() = default;
