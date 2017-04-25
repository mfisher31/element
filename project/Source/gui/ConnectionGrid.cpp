/*
    This file is part of the element modules for the JUCE Library
    Copyright (C) 2016  Kushview, LLC.  All rights reserved.
*/

#include "ElementApp.h"
#include "engine/GraphProcessor.h"
#include "gui/HorizontalListBox.h"
#include "gui/ViewHelpers.h"
#include "gui/ContentComponent.h"
#include "gui/GuiApp.h"
#include "gui/ViewHelpers.h"
#include "session/PluginManager.h"
#include "session/NodeModel.h"
#include "Messages.h"

#include "gui/ConnectionGrid.h"

namespace Element {
    // Spacing between each patch point
    static const int gridPadding = 1;
    
    bool connectionExists (const ValueTree& arcs, const uint32 sourceNode, const uint32 sourcePort,
                                                  const uint32 destNode, const uint32 destPort)
    {
        for (int i = arcs.getNumChildren(); --i >= 0;)
        {
            const ValueTree arc (arcs.getChild (i));
            if (static_cast<int> (sourceNode) == (int) arc.getProperty (Tags::sourceNode) &&
                static_cast<int> (sourcePort) == (int) arc.getProperty (Tags::sourcePort) &&
                static_cast<int> (destNode) == (int) arc.getProperty (Tags::destNode) &&
                static_cast<int> (destPort) == (int) arc.getProperty (Tags::destPort))
            {
                return true;
            }
        }
        
        return false;
    }
    
    class NodePopupMenu : public PopupMenu {
    public:
        enum ItemIds
        {
            Duplicate = 1,
            RemoveNode,
            AddAudioInputNode,
            AddAudioOutputNode,
            LastItem
        };
        
        NodePopupMenu (const Node& n)
            : node (n)
        {
            for (int item = RemoveNode; item < LastItem; ++item) {
                if (item == AddAudioInputNode)
                    addSeparator();
                addItem (item, getNameForItem ((ItemIds) item));
            }
        }
        
        
        const Node node;
    private:
        
        String getNameForItem (ItemIds item)
        {
            switch (item)
            {
                case Duplicate:  return "Duplicate"; break;
                case RemoveNode: return "Remove"; break;
                case AddAudioInputNode: return "Add audio inputs"; break;
                case AddAudioOutputNode: return "Add audio outputs"; break;
                default: jassertfalse; break;
            }
            return "Unknown Item";
        }
    };
    
    class ConnectionGrid::PatchMatrix :  public PatchMatrixComponent,
                                         private ValueTree::Listener
    {
    public:
        PatchMatrix()
            : useHighlighting(false),
              matrix()
        {
            setSize (300, 200);

            nodeModels = ValueTree (Tags::nodes);
            nodes.clear();
            nodeModels.addListener (this);
        }
        
        ~PatchMatrix()
        {
            nodeModels.removeListener (this);
        }
        
        
        const Node getNode (const int index, const bool isSource) const
        {
            return nodes [isSource ? audioOutIndexes [index]
                                   : audioInIndexes [index]];
        }
        
        const Port getPort (const int index, const bool isSource) const
        {
            const PortArray& ports = isSource ? outs : ins;
            jassert (isPositiveAndBelow (index, ports.size()));
            return ports.getUnchecked (index);
        }
        
        const int getAudioChannelForIndex (const int index, const bool isSource) const
        {
            return isSource ? audioOutChannels.getUnchecked(index)
                            : audioInChannels.getUnchecked(index);
        }
        
        void updateContent();
        
        void setUseHighlighting (const bool shouldUseHighlighting)
        {
            if (shouldUseHighlighting == useHighlighting)
                return;
            useHighlighting = shouldUseHighlighting;
            repaint();
        }
        
        void mouseMove (const MouseEvent& ev) override {
            PatchMatrixComponent::mouseMove (ev);
            repaint();
        }
        
        void paintMatrixCell (Graphics& g, const int width, const int height,
                              const int row, const int column) override
        {
            const Node src (getNode (row, true));
            const Node dst (getNode (column, false));
            
            if (useHighlighting &&
                    (mouseIsOverCell (row, column) && ! matrix.connected (row, column)))
            {
                g.setColour (Element::LookAndFeel_E1::elementBlue.withAlpha (0.3f));
                g.fillRect (0, 0, width - gridPadding, height - gridPadding);
            }
            else
            {
                g.setColour (matrix.connected (row, column) ?
                             Colour (Element::LookAndFeel_E1::elementBlue.brighter()) :
                             Colour (Element::LookAndFeel_E1::defaultMatrixCellOffColor));
        
                g.fillRect (0, 0, width - gridPadding, height - gridPadding);

            }
        }
        
        void matrixCellClicked (const int row, const int col, const MouseEvent& ev) override
        {
            const Node srcNode (getNode (row, true));
            const Port srcPort (getPort (row, true));
            const Node dstNode (getNode (col, false));
            const Port dstPort (getPort (col, false));
            
            if (ev.mods.isPopupMenu()) {
                // noop
                return;
            }
            
            if (! srcNode.canConnectTo (dstNode)) {
                matrix.disconnect (row, col);
                repaint();
                return;
            }
            
            const ValueTree arcs (srcNode.getParentArcsNode());
            if (connectionExists (arcs, srcNode.getNodeId(), srcPort.getIndex(),
                                        dstNode.getNodeId(), dstPort.getIndex()))
            {
                matrix.disconnect (row, col);
                ViewHelpers::postMessageFor (this, new RemoveConnectionMessage (
                    srcNode.getNodeId(), srcPort.getIndex(), dstNode.getNodeId(), dstPort.getIndex()));
                
            }
            else
            {
                matrix.connect (row, col);
                ViewHelpers::postMessageFor (this, new AddConnectionMessage (
                    srcNode.getNodeId(), srcPort.getIndex(), dstNode.getNodeId(), dstPort.getIndex()));
            }
            
            repaint();
        }
        
        int getNumRows()    override { return matrix.getNumRows(); }
        int getNumColumns() override { return matrix.getNumColumns(); }
        
        void paint (Graphics& g) override
        {
            if (matrix.isNotEmpty())
                return PatchMatrixComponent::paint (g);
            
            paintEmptyMessage (g, getWidth(), getHeight());
        }
        
        void showMenuForNode (const Node& node)
        {
            NodePopupMenu menu (node);
            
            switch (menu.show())
            {
                case NodePopupMenu::RemoveNode: {
                    ViewHelpers::postMessageFor (this, new RemoveNodeMessage (node));
                } break;
                    
                case NodePopupMenu::AddAudioInputNode: {
                    PluginDescription desc;
                    desc.fileOrIdentifier = "audio.input";
                    desc.pluginFormatName = "Internal";
                    ViewHelpers::postMessageFor (this, new LoadPluginMessage (desc));
                } break;
                    
                case NodePopupMenu::AddAudioOutputNode: {
                    PluginDescription desc;
                    desc.fileOrIdentifier = "audio.output";
                    desc.pluginFormatName = "Internal";
                    ViewHelpers::postMessageFor (this, new LoadPluginMessage (desc));
                } break;
            }
        }
        
    private:
        friend class ConnectionGrid;
        friend class Sources;
        friend class Destinations;
        bool useHighlighting;
        MatrixState matrix;
        ValueTree nodeModels;
        NodeArray nodes;
        PortArray ins, outs;
        Array<int> audioInIndexes, audioOutIndexes, audioInChannels, audioOutChannels;

        void paintEmptyMessage (Graphics& g, const int width, const int height)
        {
            g.setColour(LookAndFeel_E1::textColor);
            g.drawFittedText ("Nothing to see here...", 0, 0, width, height, Justification::centred, 2);
        }
        
        void paintListBoxItem (int rowNumber, Graphics& g, int width, int height,
                               bool rowIsSelected, bool isSource)
        {
            const int padding = 18;
            const Node node (getNode (rowNumber, isSource));
            const Port port (getPort (rowNumber, isSource));
            
            String text = node.getName();
            text << " - " << port.getName();
            
            g.setColour (LookAndFeel_E1::widgetBackgroundColor);
            if (isSource)
                g.fillRect (0, 0, width - 1, height - 1);
            else
                g.fillRect (0, 1, width - 1, height - 1);
            g.setColour (Colours::white);
            
            if (isSource)
            {
                g.drawText (text, padding, 0, width - 1 - padding, height - 1, Justification::centredLeft);
            }
            else
            {
                g.addTransform (AffineTransform().rotated (1.57079633f, 0, 0).translated(width, 0));
                g.drawFittedText (text, padding, 0, height - 1 - padding, width, Justification::centredLeft, 1);
            }
        }
        
        static ValueTree findArc (const ValueTree& arcs, uint32 sourceNode, int sourceChannel, uint32 destNode, int destChannel)
        {
            for (int i = arcs.getNumChildren(); --i >= 0;)
            {
                const ValueTree arc (arcs.getChild (i));
                if (sourceNode == (uint32)(int64)arc.getProperty (Tags::sourceNode) &&
                    sourceChannel == (int) arc.getProperty (Tags::sourceChannel) &&
                    destNode == (uint32)(int64)arc.getProperty (Tags::destNode) &&
                    destChannel == (int) arc.getProperty (Tags::destChannel))
                {
                    return arc;
                }
            }
            
            return ValueTree::invalid;
        }
        
        static ValueTree findArc (const ValueTree& arcs, uint32 sourceNode, uint32 sourcePort,
                                                         uint32 destNode, uint32 destPort)
        {
            for (int i = arcs.getNumChildren(); --i >= 0;)
            {
                const ValueTree arc (arcs.getChild (i));
                if (static_cast<int> (sourceNode) == (int) arc.getProperty (Tags::sourceNode) &&
                    static_cast<int> (sourcePort) == (int) arc.getProperty (Tags::sourcePort) &&
                    static_cast<int> (destNode) == (int) arc.getProperty (Tags::destNode) &&
                    static_cast<int> (destPort) == (int) arc.getProperty (Tags::destPort))
                {
                    return arc;
                }
            }
            
            return ValueTree::invalid;
        }
        
        static ValueTree findArc (const ValueTree& arcs, const Node& sourceNode, int sourceChannel,
                                  const Node& destNode, int destChannel)
        {
            return findArc (arcs, sourceNode.getNodeId(), sourceChannel,
                                  destNode.getNodeId(), destChannel);
        }
        
        void resetMatrix()
        {
            const ValueTree arcs (nodeModels.getParent().getChildWithName(Tags::arcs));
            jassert (arcs.hasType (Tags::arcs));
            for (int row = 0; row < matrix.getNumRows(); ++row)
            {
                for (int col = 0; col < matrix.getNumColumns(); ++col)
                {
                    const Node src = getNode (row, true);
                    const Port srcPort = getPort (row, true);
                    const Node dst = getNode (col, false);
                    const Port dstPort = getPort (col, false);
                    
                    const ValueTree arc (findArc (arcs, src.getNodeId(), srcPort.getIndex(),
                                                        dst.getNodeId(), dstPort.getIndex()));
                    if (arc.isValid())
                        matrix.connect (row, col);
                    else
                        matrix.disconnect (row, col);
                }
            }
        }
        
        void buildNodeArray()
        {
            nodes.clearQuick();
            for (int i = 0; i < nodeModels.getNumChildren(); ++i)
            {
                const Node node (nodeModels.getChild(i));
                nodes.add (node);
            }
            updateContent();
        }
        
        friend class ValueTree;
        virtual void valueTreePropertyChanged (ValueTree& treeWhosePropertyHasChanged,
                                               const Identifier& property) override { }
        
        virtual void valueTreeChildAdded (ValueTree& parentTree,
                                          ValueTree& childWhichHasBeenAdded) override
        {
            if (nodeModels == parentTree && childWhichHasBeenAdded.hasType(Tags::node))
                buildNodeArray();
        }
        
        virtual void valueTreeChildRemoved (ValueTree& parentTree,
                                            ValueTree& childWhichHasBeenRemoved,
                                            int indexFromWhichChildWasRemoved) override
        {
            if (nodeModels == parentTree && childWhichHasBeenRemoved.hasType (Tags::node))
                buildNodeArray();
        }
        
        virtual void valueTreeChildOrderChanged (ValueTree& parentTreeWhoseChildrenHaveMoved,
                                                 int oldIndex, int newIndex) override { }
        
        virtual void valueTreeParentChanged (ValueTree& treeWhoseParentHasChanged) override { }
        
        virtual void valueTreeRedirected (ValueTree& treeWhichHasBeenChanged) override
        {
            if (nodeModels != treeWhichHasBeenChanged)
                return;
            buildNodeArray();
            resetMatrix();
        }
    };

    // MARK: Sources
    
    class ConnectionGrid::Sources : public ListBox,
                                    public ListBoxModel
    {
    public:
        Sources (PatchMatrix* m)
            : matrix (m)
        {
            jassert (m != nullptr);
            setRowHeight (matrix->getRowThickness());
            setModel (this);
        }
        
        ~Sources() { }
        
        int getNumRows() override { return matrix->getNumRows(); };
        
        void paintListBoxItem (int rowNumber, Graphics& g, int width, int height,
                               bool rowIsSelected) override
        {
            matrix->paintListBoxItem (rowNumber, g, width, height, rowIsSelected, true);
        }
        
        void listWasScrolled() override
        {
            if (auto *scroll = getVerticalScrollBar())
            {
                matrix->setOffsetY (-roundDoubleToInt (scroll->getCurrentRangeStart()));
                matrix->repaint();
            }
        }
        
        void listBoxItemClicked (int row, const MouseEvent& ev) override
        {
            if (! ev.mods.isPopupMenu())
                return;
            const Node node (matrix->getNode (row, true));
            matrix->showMenuForNode (node);
        }

        void listBoxItemDoubleClicked (int row, const MouseEvent&) override
        {

        }
#if 0
        virtual Component* refreshComponentForRow (int rowNumber, bool isRowSelected,
                                                   Component* existingComponentToUpdate);
        virtual void listBoxItemClicked (int row, const MouseEvent&);
        virtual void listBoxItemDoubleClicked (int row, const MouseEvent&);
        virtual void backgroundClicked (const MouseEvent&);
        virtual void selectedRowsChanged (int lastRowSelected);
        virtual void deleteKeyPressed (int lastRowSelected);
        virtual void returnKeyPressed (int lastRowSelected);
        
        virtual var getDragSourceDescription (const SparseSet<int>& rowsToDescribe);
        virtual String getTooltipForRow (int row);
        virtual MouseCursor getMouseCursorForRow (int row);
#endif
        
    private:
        PatchMatrix* matrix;
        friend class PatchMatrix;
    };
    
    // MARK: Controls
    
    class ConnectionGrid::Controls : public Component { };
    
    // MARK: Destinations
    
    class ConnectionGrid::Destinations : public HorizontalListBox,
                                         public ListBoxModel
    {
    public:
        Destinations (PatchMatrix* m)
            : matrix (m)
        {
            jassert(m != nullptr);
            setRowHeight (matrix->getColumnThickness());
            setModel (this);
        }
        
        int getNumRows() override { return matrix->getNumColumns(); }
        
        void paintListBoxItem (int rowNumber, Graphics& g, int width, int height,
                               bool rowIsSelected) override
        {
            matrix->paintListBoxItem (rowNumber, g, width, height, rowIsSelected, false);
        }
        
        void listWasScrolled() override
        {
            if (auto *scroll = getHorizontalScrollBar())
            {
                matrix->setOffsetX (-roundDoubleToInt (scroll->getCurrentRangeStart()));
                matrix->repaint();
            }
        }

        void listBoxItemClicked (int row, const MouseEvent& ev) override
        {
            if (! ev.mods.isPopupMenu())
                return;
            const Node node (matrix->getNode (row, true));
            matrix->showMenuForNode (node);
        }
        
        void listBoxItemDoubleClicked (int row, const MouseEvent&) override
        {
            
        }
        
    private:
        PatchMatrix* matrix;
        friend class PatchMatrix;
    };
    
    // MARK: Quads
    
    class ConnectionGrid::Quads : public QuadrantLayout
    {
    public:
        Quads() : thicknessOnOtherQuads (190) { }
        ~Quads() { }
        
        void updateCenter() override
        {
            // keeps q2, q3, and q4 static
            const int w = getWidth();
            const int h = getHeight();
            const int x = (thicknessOnOtherQuads <= w) ? thicknessOnOtherQuads : 0;
            const int y = (h - thicknessOnOtherQuads >= 0) ? h - thicknessOnOtherQuads : 0;
            setCenter (x, y);
        }
        
    private:
        int thicknessOnOtherQuads;
    };
    
    // MARK: PatchMatrix IMPL
    
    void ConnectionGrid::PatchMatrix::updateContent()
    {
        audioInIndexes.clearQuick(); audioOutIndexes.clearQuick();
        audioInChannels.clearQuick(); audioOutChannels.clearQuick();
        ins.clearQuick(); outs.clearQuick();
        int newNumRows = 0, newNumCols = 0, nodeIndex = 0;

        for (const Node& node : nodes)
        {
            const ValueTree ports (node.getPortsValueTree());
            for (int i = 0; i < ports.getNumChildren(); ++i)
            {
                const Port port (ports.getChild (i));
                if (port.getType() != PortType::Audio)
                    continue;
            
                if (port.isInput())
                {
                    audioInIndexes.add (nodeIndex);
                    audioInChannels.add (i);
                    ins.add(port);
                    ++newNumCols;
                }
                else
                {
                    audioOutIndexes.add (nodeIndex);
                    audioOutChannels.add (i);
                    outs.add(port);
                    ++newNumRows;
                }
            }
            
            ++nodeIndex;
        }
        
        matrix.resize (newNumRows, newNumCols);
        jassert(newNumRows == outs.size() &&
                newNumCols == ins.size());
        
        resetMatrix();
        
        if (auto* grid = findParentComponentOfClass<ConnectionGrid>())
        {
            grid->sources->updateContent();
            grid->destinations->updateContent();
        }
        
        repaint();
    }

    // MARK: Connection Grid IMPL
    
    ConnectionGrid::ConnectionGrid()
    {
        addAndMakeVisible (quads = new Quads());
        quads->setQuadrantComponent (Quads::Q1, matrix = new PatchMatrix());
        quads->setQuadrantComponent (Quads::Q2, sources = new Sources (matrix));
        quads->setQuadrantComponent (Quads::Q3, controls = new Controls());
        quads->setQuadrantComponent (Quads::Q4, destinations = new Destinations (matrix));
        resized();
    }
    
    ConnectionGrid::~ConnectionGrid()
    {
        matrix = nullptr;
        sources = nullptr;
        controls = nullptr;
        destinations = nullptr;
        quads = nullptr;
    }
    
    void ConnectionGrid::setNode (const Node& newNode)
    {
        jassert (newNode.hasNodeType (Tags::graph)); // need a graph at the moment
        jassert (this->matrix != nullptr);
        matrix->nodeModels = newNode.getNodesValueTree();
    }
    
    void ConnectionGrid::paint (Graphics& g)
    {
        g.fillAll (LookAndFeel_E1::contentBackgroundColor);
    }
    
    void ConnectionGrid::resized()
    {
        quads->setBounds (getLocalBounds());
    }
    
    bool ConnectionGrid::isInterestedInDragSource (const SourceDetails& sd)
    {
        return sd.description.isArray() &&
               sd.description.size() == 3 &&
               sd.description[0].toString() == "element://dnd/plugin";
    }
    
    void ConnectionGrid::itemDropped (const SourceDetails& sd)
    {
        PluginDescription desc;
        desc.pluginFormatName = sd.description[1];
        desc.fileOrIdentifier = sd.description[2];
        ViewHelpers::postMessageFor (this, new LoadPluginMessage (desc));
    }
}