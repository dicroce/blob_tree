# blob_tree

The goal of blob_tree is to bring the heirarchical tree like structure of JSON to the world of binary data. Put the header wherever is convenient for your project.

```cpp
#include "blob_tree.h"
#include <assert.h>
using namespace std;
int main(int argc, char* argv[])
{
    dicroce::blob_tree bt;

    vector<uint8_t> b1 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    vector<uint8_t> b2 = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

    // Create a node called "blobs" and store two blobs ("b1" and "b2") under it.
    bt["blobs"]["b1"] = make_pair(b1.size(), &b1[0]); // data is inserted as a pair of pointer and size variables
    bt["blobs"]["b2"] = make_pair(b2.size(), &b2[0]);

    vector<uint8_t> b3 = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    vector<uint8_t> b4 = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

    // Blobs can also be stored by array indexes.
    bt["blobs"]["container"][0] = make_pair(b3.size(), &b3[0]);
    bt["blobs"]["container"][1] = make_pair(b4.size(), &b4[0]);
    
    // Note: You can get the size of the array like this:
    // bt["blobs"]["container"].size()

    // serialize() stores the whole blob tree and every added item to a vector
    // of bytes.
    vector<uint8_t> serialized = dicroce::blob_tree::serialize(bt, 1);

    uint32_t version;
    auto bt2 = dicroce::blob_tree::deserialize(&serialized[0], serialized.size(), version);

    // fetching an item from the blob tree...
    auto val = bt2["blobs"]["b1"].get();

    assert(memcmp(val.second, &b1[0], val.first) == 0);

    return 0;
}
```
