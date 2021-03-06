/*
 *$Log: perfhlib.c,v $
 * Revision 1.5  93/09/29  14:45:02  emmerik
 * Oops, didn't do the casts last check in
 *
 * Revision 1.4  93/09/29  14:41:45  emmerik
 * Added casts to mod instructions to keep the SVR4 compiler happy
 *
 *
 * Perfect hashing function library. Contains functions to generate perfect
 * hashing functions
 */
#include "perfhlib.h"
#include "PatternCollector.h"

#include <stdio.h>
#include <cassert>
#include <stdlib.h>
#include <string.h>
/* Private data structures */

//static  int     NumEntry;   /* Number of entries in the hash table (# keys) */
//static  int     EntryLen;   /* Size (bytes) of each entry (size of keys) */
//static  int     SetSize;    /* Size of the char set */
//static  char    SetMin;     /* First char in the set */
//static  int     NumVert;    /* c times NumEntry */

//static  uint16_t    *T1base, *T2base;   /* Pointers to start of T1, T2 */
static  uint16_t    *T1, *T2;   /* Pointers to T1[i], T2[i] */

static  int     *graphNode; /* The array of edges */
static  int     *graphNext; /* Linked list of edges */
static  int     *graphFirst;/* First edge at a vertex */


static  int     numEdges;   /* An edge counter */
static  bool    *visited;   /* Array of bools: whether visited */
static  bool    *deleted;   /* Array of bools: whether deleted */

/* Private prototypes */
static void initGraph(void);
static void addToGraph(int e, int v1, int v2);
static bool isCycle(void);
static void duplicateKeys(int v1, int v2);

void PerfectHash::setHashParams(int _NumEntry, int _EntryLen, int _SetSize, char _SetMin,
                                int _NumVert)
{
    /* These parameters are stored in statics so as to obviate the need for
        passing all these (or defererencing pointers) for every call to hash()
    */

    NumEntry = _NumEntry;
    EntryLen = _EntryLen;
    SetSize  = _SetSize;
    SetMin   = _SetMin;
    NumVert  = _NumVert;

    /* Allocate the variable sized tables etc */
    if ((T1base = (uint16_t *)malloc(EntryLen * SetSize * sizeof(uint16_t))) == 0)
    {
        goto BadAlloc;
    }
    if ((T2base = (uint16_t *)malloc(EntryLen * SetSize * sizeof(uint16_t))) == 0)
    {
        goto BadAlloc;
    }

    if ((graphNode = (int *)malloc((NumEntry*2 + 1) * sizeof(int))) == 0)
    {
        goto BadAlloc;
    }
    if ((graphNext = (int *)malloc((NumEntry*2 + 1) * sizeof(int))) == 0)
    {
        goto BadAlloc;
    }
    if ((graphFirst = (int *)malloc((NumVert + 1) * sizeof(int))) == 0)
    {
        goto BadAlloc;
    }

    if ((g = (short *)malloc((NumVert+1) * sizeof(short))) == 0)
    {
        goto BadAlloc;
    }
    if ((visited = (bool *)malloc((NumVert+1) * sizeof(bool))) == 0)
    {
        goto BadAlloc;
    }
    if ((deleted = (bool *)malloc((NumEntry+1) * sizeof(bool))) == 0)
    {
        goto BadAlloc;
    }
    return;

BadAlloc:
    printf("Could not allocate memory\n");
    hashCleanup();
    exit(1);
}

void PerfectHash::hashCleanup(void)
{
    /* Free the storage for variable sized tables etc */
    if (T1base) free(T1base);
    if (T2base) free(T2base);
    if (graphNode) free(graphNode);
    if (graphNext) free(graphNext);
    if (graphFirst) free(graphFirst);
    if (g) free(g);
    if (visited) free(visited);
    if (deleted) free(deleted);
}

void PerfectHash::map(PatternCollector *collector)
{
    m_collector = collector;
    assert(nullptr!=collector);
    int i, j, c;
    uint16_t f1, f2;
    bool cycle;
    uint8_t *keys;

    c = 0;

    do
    {
        initGraph();
        cycle = false;

        /* Randomly generate T1 and T2 */
        for (i=0; i < SetSize*EntryLen; i++)
        {
            T1base[i] = rand() % NumVert;
            T2base[i] = rand() % NumVert;
        }

        for (i=0; i < NumEntry; i++)
        {
            f1 = 0; f2 = 0;
            keys = m_collector->getKey(i);
            for (j=0; j < EntryLen; j++)
            {
                T1 = T1base + j * SetSize;
                T2 = T2base + j * SetSize;
                f1 += T1[keys[j] - SetMin];
                f2 += T2[keys[j] - SetMin];
            }
            f1 %= (uint16_t)NumVert;
            f2 %= (uint16_t)NumVert;
            if (f1 == f2)
            {
                /* A self loop. Reject! */
                printf("Self loop on vertex %d!\n", f1);
                cycle = true;
                break;
            }
            addToGraph(numEdges++, f1, f2);
        }
        if (cycle || (cycle = isCycle()))   /* OK - is there a cycle? */
        {
            printf("Iteration %d\n", ++c);
        }
        else
        {
            break;
        }
    }
    while (/* there is a cycle */ 1);

}

/* Initialise the graph */
void PerfectHash::initGraph()
{
    int i;

    for (i=1; i <= NumVert; i++)
    {
        graphFirst[i] = 0;
    }

    for (i= -NumEntry; i <= NumEntry; i++)
    {
        /* No need to init graphNode[] as they will all be filled by successive
            calls to addToGraph() */
        graphNext[NumEntry+i] = 0;
    }

    numEdges = 0;
}

/* Add an edge e between vertices v1 and v2 */
/* e, v1, v2 are 0 based */
void PerfectHash::addToGraph(int e, int v1, int v2)
{
    e++; v1++; v2++;                        /* So much more convenient */

    graphNode[NumEntry+e] = v2;             /* Insert the edge information */
    graphNode[NumEntry-e] = v1;

    graphNext[NumEntry+e] = graphFirst[v1]; /* Insert v1 to list of alphas */
    graphFirst[v1]= e;
    graphNext[NumEntry-e] = graphFirst[v2]; /* Insert v2 to list of omegas */
    graphFirst[v2]= -e;

}

bool PerfectHash::DFS(int parentE, int v)
{
    int e, w;

    /* Depth first search of the graph, starting at vertex v, looking for
        cycles. parent and v are origin 1. Note parent is an EDGE,
        not a vertex */

    visited[v] = true;

    /* For each e incident with v .. */
    for (e = graphFirst[v]; e; e = graphNext[NumEntry+e])
    {
        uint8_t *key1;

        if (deleted[abs(e)])
        {
            /* A deleted key. Just ignore it */
            continue;
        }
        key1 = m_collector->getKey(abs(e)-1);
        w = graphNode[NumEntry+e];
        if (visited[w])
        {
            /* Did we just come through this edge? If so, ignore it. */
            if (abs(e) != abs(parentE))
            {
                /* There is a cycle in the graph. There is some subtle code here
                    to work around the distinct possibility that there may be
                    duplicate keys. Duplicate keys will always cause unit
                    cycles, since f1 and f2 (used to select v and w) will be the
                    same for both. The edges (representing an index into the
                    array of keys) are distinct, but the key values are not.
                    The logic is as follows: for the candidate edge e, check to
                    see if it terminates in the parent vertex. If so, we test
                    the keys associated with e and the parent, and if they are
                    the same, we can safely ignore e for the purposes of cycle
                    detection, since edge e adds nothing to the cycle. Cycles
                    involving v, w, and e0 will still be found. The parent
                    edge was not similarly eliminated because at the time when
                    it was a candidate, v was not yet visited.
                    We still have to remove the key from further consideration,
                    since each edge is visited twice, but with a different
                    parent edge each time.
                */
                /* We save some stack space by calculating the parent vertex
                    for these relatively few cases where it is needed */
                int parentV = graphNode[NumEntry-parentE];

                if (w == parentV)
                {
                    uint8_t *key2;

                    key2=m_collector->getKey(abs(parentE)-1);
                    if (memcmp(key1, key2, EntryLen) == 0)
                    {
                        printf("Duplicate keys with edges %d and %d (",
                               e, parentE);
                        m_collector->dispKey(abs(e)-1);
                        printf(" & ");
                        m_collector->dispKey(abs(parentE)-1);
                        printf(")\n");
                        deleted[abs(e)] = true;      /* Wipe the key */
                    }
                    else
                    {
                        /* A genuine (unit) cycle. */
                        printf("There is a unit cycle involving vertex %d and edge %d\n", v, e);
                        return true;
                    }

                }
                else
                {
                    /* We have reached a previously visited vertex not the
                        parent. Therefore, we have uncovered a genuine cycle */
                    printf("There is a cycle involving vertex %d and edge %d\n", v, e);
                    return true;

                }
            }
        }
        else                                /* Not yet seen. Traverse it */
        {
            if (DFS(e, w))
            {
                /* Cycle found deeper down. Exit */
                return true;
            }
        }
    }
    return false;
}

bool PerfectHash::isCycle(void)
{
    int v, e;

    for (v=1; v <= NumVert; v++)
    {
        visited[v] = false;
    }
    for (e=1; e <= NumEntry; e++)
    {
        deleted[e] = false;
    }
    for (v=1; v <= NumVert; v++)
    {
        if (!visited[v])
        {
            if (DFS(-32767, v))
            {
                return true;
            }
        }
    }
    return false;
}

void PerfectHash::traverse(int u)
{
    int w, e;

    visited[u] = true;
    /* Find w, the neighbours of u, by searching the edges e associated with u */
    e = graphFirst[1+u];
    while (e)
    {
        w = graphNode[NumEntry+e]-1;
        if (!visited[w])
        {
            g[w] = (abs(e)-1 - g[u]) % NumEntry;
            if (g[w] < 0) g[w] += NumEntry;     /* Keep these positive */
            traverse(w);
        }
        e = graphNext[NumEntry+e];
    }

}

void PerfectHash::assign(void)
{
    int v;


    for (v=0; v < NumVert; v++)
    {
        g[v] = 0;                           /* g is sparse; leave the gaps 0 */
        visited[v] = false;
    }

    for (v=0; v < NumVert; v++)
    {
        if (!visited[v])
        {
            g[v] = 0;
            traverse(v);
        }
    }
}

int PerfectHash::hash(uint8_t *string)
{
    uint16_t u, v;
    int  j;

    u = 0;
    for (j=0; j < EntryLen; j++)
    {
        T1 = T1base + j * SetSize;
        u += T1[string[j] - SetMin];
    }
    u %= NumVert;

    v = 0;
    for (j=0; j < EntryLen; j++)
    {
        T2 = T2base + j * SetSize;
        v += T2[string[j] - SetMin];
    }
    v %= NumVert;

    return (g[u] + g[v]) % NumEntry;
}

#if 0
void dispRecord(int i);

void
duplicateKeys(int v1, int v2)
{
    int i, j;
    uint8_t *keys;
    int u, v;

    v1--; v2--;             /* These guys are origin 1 */

    printf("Duplicate keys:\n");

    for (i=0; i < NumEntry; i++)
    {
        getKey(i, &keys);
        u = 0;
        for (j=0; j < EntryLen; j++)
        {
            T1 = T1base + j * SetSize;
            u += T1[keys[j] - SetMin];
        }
        u %= NumVert;
        if ((u != v1) && (u != v2)) continue;

        v = 0;
        for (j=0; j < EntryLen; j++)
        {
            T2 = T2base + j * SetSize;
            v += T2[keys[j] - SetMin];
        }
        v %= NumVert;

        if ((v == v2) || (v == v1))
        {
            printf("Entry #%d key: ", i+1);
            for (j=0; j < EntryLen; j++) printf("%02X ", keys[j]);
            printf("\n");
            dispRecord(i+1);
        }
    }
    exit(1);


}
#endif
