# Doom Clone

## BSP Algorithm

1) Splitter selection
Pick a line that divides the current set of segments into "Left" and "Right" sets.

2) Sort and Split Segments

Iterate through the list of segments and classify them against the Splitter Line:
    On Left: Add to Left List.
    On Right: Add to Right List.

    Spanning (Crossing): Cut the segment into two new Segs.
        Calculate intersection point I.
        Create Seg A: Start -> I (add to side A).
        Create Seg B: I -> End (add to side B).
        Create a new Vertex at I.

Step C: Recursion / Base Case
    Base Case: If the list of segments describes a convex area (or is empty), stop.
        Create a Subsector.
        Store the list of Segs into the global SEGS lump.
        Return the index of this new Subsector (with the high bit set, see below).

    Recursive Step:
        leftChild = BuildBSPTree(leftSegments)
        rightChild = BuildBSPTree(rightSegments)
        Create a Node.
        Calculate Bounding Boxes for left and right sets.
        Return the Node index.
