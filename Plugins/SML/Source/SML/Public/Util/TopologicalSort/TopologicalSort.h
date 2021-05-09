#pragma once
#include "CoreMinimal.h"
#include "Util/TopologicalSort/DirectedGraph.h"

/**
 * Handles topological sorting of the directed graph
 */
class FTopologicalSort {
public:
    template<typename T>
    static bool ExploreNode(const T& Node, const TDirectedGraph<T>& Graph, TArray<T>& SortedResult, TSet<T>& VisitedNodes, TSet<T>& ExpandedNodes, TSet<T>* OutCycleNodes) {
        // Have we been here before?
        if (VisitedNodes.Contains(Node)) {
            // And have completed this node before
            if (ExpandedNodes.Contains(Node)) {
                // Then we're fine
                return true;
            }
            // It's a cycle in the input graph, so sorting is not possible, faulty nodes are the difference between already visited but not yet expanded nodes
            const TSet<T> CycleList = VisitedNodes.Difference(ExpandedNodes);
            if (OutCycleNodes) {
                OutCycleNodes->Append(CycleList);
            }
            return false;
        }

        // Visit this node
        VisitedNodes.Add(Node);

        // Recursively explore inbound edges
        bool bVisitedChildren = true;
        for (const T& Inbound : Graph.EdgesFrom(Node)) {
            bVisitedChildren &= ExploreNode(Inbound, Graph, SortedResult, VisitedNodes, ExpandedNodes, OutCycleNodes);
        }

        // Add ourselves now
        SortedResult.Add(Node);
        // And mark ourselves as explored
        ExpandedNodes.Add(Node);
        return bVisitedChildren;
    }

    /**
     * Performs a topological dependency sorting on a provided directed graph
     *
     * @param Graph graph to perform topological sort on
     * @param OutSortedNodes sorted nodes of the graph will be emitted into that array
     * @param OutCycleNodes pointer to the array in which cycle nodes will be reported
     * @return true if sorting was successful (e.g no cycle nodes were encountered), false otherwise
     */
    template<typename T>
    static bool TopologicalSort(const TDirectedGraph<T>& Graph, TArray<T>& OutSortedNodes, TSet<T>* OutCycleNodes = NULL) {
        TDirectedGraph<T> ReversedGraph = Graph.InverseGraph();
        TSet<T> VisitedNodes;
        // A list of "fully explored" nodes. Leftovers in here indicate cycles in the graph
        TSet<T> ExpandedNodes;
        bool bSortingSuccess = true;

        for (const T& Node : ReversedGraph.OrderedNodes) {
            bSortingSuccess &= ExploreNode(Node, ReversedGraph, OutSortedNodes, VisitedNodes, ExpandedNodes, OutCycleNodes);
        }
        return bSortingSuccess;
    }
};
