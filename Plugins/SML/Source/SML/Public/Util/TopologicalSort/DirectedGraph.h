#pragma once
#include "CoreTypes.h"

/**
* Represents simple directional graph of some value type
* For memory efficiency, graph is represented via adjacent node list
* Values passed into methods are copied internally
*/
template<typename T>
class TDirectedGraph {
public:
    // use a pointer to the unordered set as value to avoid excessive copying
    TMap<T, TSet<T>> Graph;
    TArray<T> OrderedNodes;
public:
    /**
    * Adds node into the graph without any adjacent nodes
    * returns false if specified node already exists in a graph
    * returns true otherwise
    */
    FORCEINLINE bool AddNode(const T& Node) {
        if (!Graph.Contains(Node)) {
            OrderedNodes.Add(Node);
            Graph.FindOrAdd(Node);
            return true;
        }
        return false;
    }

    /**
    * Adds node adjacent to src into the graph
    * If either of the nodes is not in a graph, throws std::invalid_argument
    */
    FORCEINLINE bool AddEdge(const T& From, const T& To) {
        if (Graph.Contains(From) && Graph.Contains(To)) {
            TSet<T>& FoundSet = Graph.FindChecked(From);
            bool bIsAlreadyInSet = false;
            FoundSet.Add(To, &bIsAlreadyInSet);
            return !bIsAlreadyInSet;
        }
        return false;
    }

    /**
    * Returns all nodes adjacent to the given one
    * Returned reference is valid as long as the this graph object is valid
    */
    FORCEINLINE const TSet<T>& EdgesFrom(const T& Node) const {
        return Graph.FindChecked(Node);
    }

    /**
     * Returns the list of the nodes in the graph
     */
    FORCEINLINE const TArray<T>& GetNodes() const {
        return OrderedNodes;
    }

    /**
    * Returns amount of nodes in the graph
    */
    FORCEINLINE int32 Num() const {
        return OrderedNodes.Num();
    }

    /** Inverses the graph */
    FORCEINLINE TDirectedGraph<T> InverseGraph() const {
        TDirectedGraph<T> Result;
        for (const T& Node : GetNodes()) {
            Result.AddNode(Node);
        }
        for (const T& From : GetNodes()) {
            for (const T& To : EdgesFrom(From)) {
                Result.AddEdge(To, From);
            }
        }
        return Result;
    }
};
