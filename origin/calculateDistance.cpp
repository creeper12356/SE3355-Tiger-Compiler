/**
 * A simple implementation of the min edge algorithm.
 * We have two edges, while someone else has only one edge. Our goal is to determine the minimum distance among these three edges.
 * Input format:
 * One value n is given.
 * Note: 1 <= n <= 1000
 * Output format:
 * Output the shortest distance between the three edges and the sum of the three distances.
 */

#include <stdio.h>

typedef int Distance;           // define distance type
typedef long long LongDistance; // define distance type
typedef struct Edge_s Edge;     // define edge type
typedef int NodeId;             // define node id type
typedef int EdgeId;             // define edge id type

struct Edge_s
{
    NodeId u, v;
    LongDistance w;
};

const EdgeId NUM = 3;               // number of directed edges
Edge edge1 = {0, 0, 5};             // define the first edge
Edge edge2 = {0, 0, 10};            // define the second edge
Distance allDist[NUM][NUM] = {0};   // adjacency matrix of the graph
Edge *dist[NUM] = {&edge1, &edge2}; // all distances
LongDistance minDistance = 5; // the shortest distance between two adjacent nodes

void caculateDistance()
{
    for (NodeId k = 0; k < NUM; k++)
    {
        LongDistance kDist = dist[k]->w;
        minDistance = kDist < minDistance ? kDist : minDistance;
    }
}

int main()
{
    // get edge
    Edge edge;
    scanf("%lld", &edge.w);
    dist[NUM - 1] = &edge;

    // store the edge in the graph, here we need to CAST LongDistance to Distance
    allDist[0][0] = edge.w;

    caculateDistance(); // calculate the shortest distance between two adjacent nodes

    printf("%lld %lld %d\n", minDistance, edge.w + 5 + 10, allDist[0][0]); // output the distance
    return 0;
}