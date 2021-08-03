#include <stdio.h>
#include <stdlib.h>

// For testing
#include <time.h>
#include <math.h>

/* treap.c
 *
 * Test code for a Treap (BST/heap hybrid that approximates self-balancing)
 *
 * Testing suggests that we can expect a maximum tree depth of 2*log(n); even
 * if the inputs are in ascending (worst-case) insertion order.
 *
 * written December 2019 (?) by Thomas Pinkava 
*/



// A Node in the Treap
typedef struct treap_node {

    unsigned int treeKey;   // The node's formal order for searching
    unsigned int heapKey;   // The node's pseudorandom priority for Treaping
                            // Max heap, larger values are closer to root

    struct treap_node *L, *R, *P;    // The "Parent" is NULL if this is the Root Node

} treap_node_t;


// Having the treap be its own struct saves weirdness with backpointers
typedef struct treap {

    treap_node_t* root;
    // TODO: lock here for threadsafing; hand-over-hand would require four locks and would
    //       be hell on toast for deadlocking concerns

} treap_t;



// Performs either a Left-Rotation or a Right-Rotation between the two nodes in the indicated treap,
// based on their treeKey values. "Root" is one that is closer to root and will be moved further out;
// "Pivot" is the child of "Root" that will take its place.
void treapRotate(treap_t *treap, treap_node_t* root, treap_node_t* pivot){
    if(pivot->treeKey < root->treeKey){
        // Right-rotation
        if(pivot->R != NULL) pivot->R->P = root;
        root->L = pivot->R;
        pivot->R = root;
    } else {
        // Left-rotation
        if(pivot->L != NULL) pivot->L->P = root;
        root->R = pivot->L;
        pivot->L = root;
    }
    // Ops common to both rotations
    pivot->P = root->P;
    if(root->P == NULL){        
        // root is treap root
        treap->root = pivot;
    } else if(root->treeKey < root->P->treeKey){
        // root is left child of parent
        root->P->L = pivot;
    } else {
        // root is right child of parent
        root->P->R = pivot;
    }
    root->P = pivot;
}




// Does the bleeding obvious; returns NULL if unfound.
treap_node_t *treapFind(treap_t *treap, unsigned int key){
    treap_node_t *cur = treap->root;
    while(cur != NULL){
        if(key < cur->treeKey){
            cur = cur->L;
        } else if (key > cur->treeKey){
            cur = cur->R;
        } else {
            return cur;
        }
    }
    return NULL;
}


// Like treapFind, but causes the found node to rise in the heap order
// so that, by principle of locality, it is swiftly found again if popular.
// TODO: Threadsafing considerations, this is a mutating operation
treap_node_t *treapUsurpingFind(treap_t *treap, unsigned int key){
    // Find the node as before
    treap_node_t *cur = treapFind(treap, key);
    // Usurp the node's parent if the node exists and is not root
    if(cur != NULL && cur->P != NULL){
        // Switch heapKeys to preserve heap order
        unsigned int tempKey = cur->heapKey;
        cur->heapKey = cur->P->heapKey;
        cur->P->heapKey = tempKey;
        treapRotate(treap, cur->P, cur);
    }
    return cur;
}



// Add a new node to the treap OR checks to see if it already exists
// Returns a pointer to the node, whether it was newly created or already exists
// TODO: some way of informing the invoker whether the node was newly added or not?
//       unless we want to give the treap a dictionary-style frontend...
treap_node_t *treapAppend(treap_t *treap, unsigned int key){

    // Binary seek to the location of the new node
    treap_node_t* cur = treap->root;
    treap_node_t **inPointer = &(treap->root); // The pointer to modify when we've made our node

    // Check to see if treap is empty
    if (cur != NULL){
        treap_node_t* next;
        while((next = (key < cur->treeKey)?cur->L:cur->R) != NULL) cur = next; 
        // Now cur points to the 'parent' node, and next is the pointer
        if(key == cur->treeKey){
            // Desired node already exists
            return cur;
        } else {
            inPointer = (key < cur->treeKey)?&(cur->L):&(cur->R);
        }
    }

    // Generate a pseudo-random heap key
    unsigned int heapKey = rand();

    // New node is allocated and inserted
    treap_node_t* newNode = (treap_node_t *)malloc(sizeof(treap_node_t));
    newNode->P = cur;
    newNode->L = NULL;
    newNode->R = NULL;
    newNode->treeKey = key;
    newNode->heapKey = heapKey;
    *inPointer = newNode;
    
    
    // Now perform priority rotations to ensure the node is in the right heap place
    while(newNode->P != NULL && newNode->heapKey > newNode->P->heapKey){
        treapRotate(treap, newNode->P, newNode); 
    }

    // Finally hand back the new node
    return newNode;
}



// remove a node from the treap
// TODO: a version of this solely by key?
void treapDecouple(treap_t *treap, treap_node_t *node){
    // If Both Children are present then downswap until we reach a stable case
    while(!(node->L == NULL || node->R == NULL)){
        if(node->L->heapKey > node->R->heapKey){
            // Swap with Left Child
            treapRotate(treap, node, node->L);
        } else {
            // Swap with Right Child
            treapRotate(treap, node, node->R);
        }
    }

    // We've reached a case with one or fewer children (safe to decouple)
    treap_node_t **inPointer;
    if(node->P == NULL){
        // Node is root
        inPointer = &(treap->root);
    } else {
        // Node has parent
        inPointer = (node->treeKey < node->P->treeKey)? &(node->P->L) : &(node->P->R);
    }

    if (node->R != NULL){
        // Right Child Case
        *inPointer = node->R;
        node->R->P = node->P;
    } else if (node->L != NULL){
        // Left Child Case
        *inPointer = node->L;
        node->L->P = node->P;
    } else {
        // Leaf Case
        *inPointer = NULL;
    }
    // Now node is totally decoupled from the treap (but not deallocated from memory)
}









// Test Drivers
void printTreapKernel(treap_node_t * node){
    if(node != NULL){
        printf("  [");
        printTreapKernel(node->L);
        printf("]-%d-[", node->treeKey);
        printTreapKernel(node->R);
        printf("]  ");
    } else {
        printf(".");
    }
}

void printTreap(treap_t *treap){
    printTreapKernel(treap->root);
    printf("\n");
}


void testInOrder(treap_node_t *node, unsigned int *value){
    if(node->L != NULL) testInOrder(node->L, value);
    if(node->L != NULL && node->L->treeKey >= node->treeKey) *value = 0;
    if(node->R != NULL && node->R->treeKey <= node->treeKey) *value = 0;
    if(node->R != NULL) testInOrder(node->R, value);
}

unsigned int properParentTest(treap_node_t* root){
    if(root == NULL){
        return 0;
    } else {
        return properParentTest(root->L) + properParentTest(root->R) + ((root->P == NULL)?1:0);
    }
}


int getMaxHeight(treap_node_t* root) {
   int left = ((root->L == NULL) ? 0 :  1 + getMaxHeight(root->L));
   int right = ((root->R == NULL) ? 0 : 1 + getMaxHeight(root->R));
   return ((right > left) ? right : left);
}


// First test; established treap function w/ order maintenance over multiple deletes
double testOne(unsigned int times){
    printf("\nRunning %u times!\n", times);
    treap_t bob;
    bob.root = NULL;
    for(unsigned int i = 0; i < times; i++){
        treapAppend(&bob, i); 
    }
    //printTreap(&bob);

    unsigned int charlie = 1;
    testInOrder(bob.root, &charlie);
    printf("In-order?: %u\n", charlie);

    int maxDepth = getMaxHeight(bob.root);
    printf("Max Depth: %d\n", maxDepth);
    double logarithm = log2(times);
    double factor = ((double)maxDepth) / logarithm;
    printf("Log Factor: %f\n", factor);

    for(unsigned int i=times/4; i < (3 * times)/4; i++){
        treap_node_t * bill = treapFind(&bob, i);
        if( bill != NULL){
            treapDecouple(&bob, bill);
            free(bill);
            //printf("Parent Nulls: %u\n", properParentTest(bob.root));
        } else {
            printf("Not found!\n");
            exit(2);
        }
    }

    charlie = 1;
    testInOrder(bob.root, &charlie);
    printf("Post-deletions: In order? %d\n", charlie);

    printf("Max Depth: %d\n", getMaxHeight(bob.root));
    return factor;
}

// Second test: assesses locality prioritization
void testTwo(void){
    treap_t bob;
    bob.root = NULL;

    for(unsigned int i = 0; i < 10; i++){
        treapAppend(&bob, i);
    }
    printTreap(&bob);

    for(int i = 0; i < 20; i++){
        treapUsurpingFind(&bob, 1);
        treapUsurpingFind(&bob, 8);
    }

    printTreap(&bob);
    
}

int main(void){

    srand(time(0));
    
    double sum = 0.0;
    int count = 0;
    for(int j = 0; j < 20; j++){
        for(unsigned int i = 2; i < 2000000; i *= 2){
            sum += testOne(i);
            count++;
        }
    }
    printf("\n\nAverage LogTime Factor: %f\n",sum/(double)count);
    
    
    return 0;
}
