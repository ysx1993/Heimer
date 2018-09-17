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

#ifndef MEDIATOR_HPP
#define MEDIATOR_HPP

#include <QObject>
#include <QPointF>
#include <QString>

#include "node.hpp"

class DragAndDropStore;
class EditorData;
class EditorScene;
class EditorView;
class MainWindow;
class QGraphicsItem;

/*! Acts as a communication channel between MainWindow and editor components:
 *
 *  - MainWindow <-> Mediator <-> QGraphicsScene / EditorView / EditorData
 *  - EditorView <-> Mediator <-> EditorData
 */
class Mediator : public QObject
{
    Q_OBJECT

public:

    explicit Mediator(MainWindow & mainWindow);

    ~Mediator();

    void addItem(QGraphicsItem & item);

    bool canBeSaved() const;

    void connectEdgeToUndoMechanism(EdgePtr edge);

    void connectNodeToUndoMechanism(NodePtr node);

    // Create a new node and add edge to the source (parent) node
    NodeBasePtr createAndAddNode(int sourceNodeIndex, QPointF pos);

    // Create a new floating node
    NodeBasePtr createAndAddNode(QPointF pos);

    DragAndDropStore & dadStore();

    void deleteNode(Node & node);

    QString fileName() const;

    NodeBasePtr getNodeByIndex(int index);

    bool hasNodes() const;

    void initializeNewMindMap();

    void initializeView();

    bool isInBetween(Node & node);

    bool isLeafNode(Node & node);

    bool isUndoable() const;

    bool isRedoable() const;

    bool isModified() const;

    int nodeCount() const;

    bool openMindMap(QString fileName);

    void redo();

    void removeItem(QGraphicsItem & item);

    bool saveMindMapAs(QString fileName);

    bool saveMindMap();

    QSize sceneRectSize() const;

    Node * selectedNode() const;

    void setEditorData(std::shared_ptr<EditorData> editorData);

    void setEditorScene(std::shared_ptr<EditorScene> editorScene);

    void setEditorView(EditorView & editorView);

    void setSelectedNode(Node * node);

    void setupMindMapAfterUndoOrRedo();

    void undo();

public slots:

    void clearScene();

    void enableUndo(bool enable);

    void exportToPNG(QString filename, QSize size, bool transparentBackground);

    void saveUndoPoint();

    void setBackgroundColor(QColor color);

    QSize zoomForExport();

private slots:

    void zoomIn();

    void zoomOut();

    void zoomToFit();

signals:

    void exportFinished();

private:

    void addExistingGraphToScene();

    void connectGraphToUndoMechanism();

    std::shared_ptr<EditorData> m_editorData;

    std::shared_ptr<EditorScene> m_editorScene;

    EditorView * m_editorView = nullptr;

    MainWindow & m_mainWindow;
};

#endif // MEDIATOR_HPP
