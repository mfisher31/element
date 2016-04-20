/*
    GraphEditorComponent.cpp - This file is part of Element
    Copyright (C) 2016 Kushview, LLC.  All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef EL_GRAPH_EDITOR_COMPONENT_H
#define EL_GRAPH_EDITOR_COMPONENT_H

#include "element/Juce.h"
#include "engine/GraphProcessor.h"

namespace Element {

class GraphController;
class FilterComponent;
class ConnectorComponent;
class PinComponent;
class PluginWindow;

/** A panel that displays and edits a GraphProcessor. */
class GraphEditorComponent   : public Component,
                               public ChangeListener,
                               public DragAndDropTarget
{
public:
    GraphEditorComponent (GraphController&);
    virtual ~GraphEditorComponent();

    void paint (Graphics& g);

    virtual void mouseDown (const MouseEvent& e);

    void createNewPlugin (const PluginDescription* desc, int x, int y);

    FilterComponent* getComponentForFilter (const uint32 filterID) const;
    ConnectorComponent* getComponentForConnection (const Arc& conn) const;
    PinComponent* findPinAt (const int x, const int y) const;

    void resized();
    void changeListenerCallback (ChangeBroadcaster*);
    void onGraphChanged();
    void updateComponents();

    void beginConnectorDrag (const uint32 sourceFilterID, const int sourceFilterChannel,
                             const uint32 destFilterID, const int destFilterChannel,
                             const MouseEvent& e);
    void dragConnector (const MouseEvent& e);
    void endDraggingConnector (const MouseEvent& e);

    virtual bool isInterestedInDragSource (const SourceDetails& /*details*/);
    //virtual void itemDragEnter (const SourceDetails& dragSourceDetails);
    //virtual void itemDragMove (const SourceDetails& dragSourceDetails);
    //virtual void itemDragExit (const SourceDetails& dragSourceDetails);
    void itemDropped (const SourceDetails& details);

    virtual bool shouldDrawDragImageWhenOver() { return true; }

protected:
    GraphController& graph;
    virtual Component* wrapAudioProcessorEditor (AudioProcessorEditor* ed, GraphNodePtr editorNode);
    
    
private:
    friend class FilterComponent;
    ScopedPointer<ConnectorComponent> draggingConnector;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphEditorComponent)
    Component* createContainerForNode (GraphNodePtr node, bool useGenericEditor);
    AudioProcessorEditor* createEditorForNode (GraphNodePtr node, bool useGenericEditor);
    PluginWindow* getOrCreateWindowForNode (GraphNodePtr f, bool useGeneric);
    void updateConnectorComponents();
};
    
typedef GraphEditorComponent GraphEditorBase;

}

#endif // EL_GRAPH_EDITOR_COMPONENT_H