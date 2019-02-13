/*
 * This is the source code of SpecNet project
 * It is licensed under MIT License.
 *
 * Copyright (c) Dmitriy Bondarenko
 * feel free to contact me: specnet.messenger@gmail.com
 */

#ifndef ONCACHE_H
#define ONCACHE_H

#include <stdlib.h>
#include <string.h>
#include <cassert>
#include <cmath>


#define SKIPHEIGHT 5
#define SKIPHEIGHT_H 2
#define SIZEAllocLeaf 256


typedef void (*TDeletDataFunc)(const char *data);

//TODO any size key implementation:
template<typename T>
union AnySizeKey {
    T key;
    static constexpr int keyLongSize = ((sizeof(T)/sizeof(long long))+ (sizeof(T)%sizeof(long long)>0?1:0));
    unsigned long long keyArray[keyLongSize];
    int cmp(AnySizeKey const & other ) {
        for (int i=0; i<keyLongSize; ++i) {
            if (keyArray[i]>other.keyArray[i]) return 1;
            if (keyArray[i]<other.keyArray[i]) return -1;
        }
        return 0;
    }
};

class TKey  {
 public:
  TKey(unsigned long long  p0,  unsigned long long  p1,  unsigned long long  p2)  {
    keyArray[0]  =  p0;
    keyArray[1]  =  p1;
    keyArray[2]  =  p2;
  }

  TKey  & operator=(TKey const  &rhl)  {
    keyArray[0]  =  rhl.keyArray[0];
    keyArray[1]  =  rhl.keyArray[1];
    keyArray[2]  =  rhl.keyArray[2];
    return  *this;
  }

  TKey(const  TKey  &rhl)  {
    keyArray[0]  =  rhl.keyArray[0];
    keyArray[1]  =  rhl.keyArray[1];
    keyArray[2]  =  rhl.keyArray[2];
  }

  bool  operator==(TKey const  &rhl) const  {
    return  keyArray[0]  ==  rhl.keyArray[0]
            && keyArray[1]  ==  rhl.keyArray[1]
            && keyArray[2]  ==  rhl.keyArray[2];
  }

  static bool  do_worst_case;
  static constexpr int  keyLongSize  =  3;
  unsigned long long  keyArray[keyLongSize];
  unsigned long long  hash() const  {
    const unsigned long long re  =  keyArray[0]  +  keyArray[1]  +  keyArray[2];
    if  (do_worst_case)  {
      //return  (re % 10);
      return  (re % 100000);
    }
    return  (re < 9223372036854775807ll)?  re  :  (re >> 1);
  }

  int  cmp(TKey const  *other)  const  {
    if  (keyArray[2]  >  other->keyArray[2])  return  1;
    if  (keyArray[2]  <  other->keyArray[2])  return  -1;
    if  (keyArray[1]  >  other->keyArray[1])  return  1;
    if  (keyArray[1]  <  other->keyArray[1])  return  -1;
    if  (keyArray[0]  >  other->keyArray[0])  return  1;
    if  (keyArray[0]  <  other->keyArray[0])  return  -1;
    return 0;
  }

  bool operator<(const TKey  &r)  const  {
    if  (keyArray[0]  <  r.keyArray[0])  return true;
    if  (keyArray[0]  >  r.keyArray[0])  return false;
    if  (keyArray[1]  <  r.keyArray[1])  return true;
    if  (keyArray[1]  >  r.keyArray[1])  return false;
    if  (keyArray[2]  <  r.keyArray[2])  return true;
    if  (keyArray[2]  >  r.keyArray[2])  return false;
        // Otherwise both are equal        
    return false;
  }
};  //  TKey

bool  TKey::do_worst_case  =  false;

inline uint qHash(const TKey *key, uint seed = 0)
{
    return key->hash() ^ seed;
}

inline uint qHash(TKey key, uint seed = 0)
{
    return key.hash() ^ seed;
}


class  TONode  {
 public:
  TONode  *fwdPtrs[SKIPHEIGHT];
    //rating queue:    
  TONode  *mostUseful;
  TONode  *leastUseful;
    //const unsigned char * key;    
  TKey const  *key;
  const char  *data;
  unsigned char  curHeight;// ==SKIPHEIGHT-1 to CPU economy
  unsigned long long  hash;
};  //  TONode


class  OnCache  {
 public:
    /*
     * capacity - how many elements can store
     * keyLen - memcmp third parameter
     * Key must be part of the stored Value - will deallocate Value only
    */    
  OnCache(unsigned int  capacity,  TDeletDataFunc f_delData)
      :  _capacity(capacity),  _f_delData(f_delData),  _hash_baskets(sqrt(capacity)),
       leafSize((_hash_baskets>256)?  _hash_baskets  :  256)  {
    init();
  }

  ~OnCache()  {
    clear();
  }

  unsigned int  size()  {
    return  _size;
  }

  const char * getData(TKey const  *key)  {
    TONode  *curFound  =  find(key) ;
    if  (curFound)  {
      toTopUsage(curFound);
      return curFound->data;
    }
    return nullptr;
  }

  void  insertNode  (TKey const  *key,  const char  *data)  {
    const unsigned long long  hash  =  key->hash();
    const unsigned short  basketID  =  hash % _hash_baskets;
    int  cmp  =  setll(hash,  key,  basketID);
    if  (0  ==  cmp)  {
      TONode  *cur  =  updatePathOut[0];
      _f_delData(cur->data);
      cur->data  =  data;
      cur->key  =  key;
      toTopUsage(cur);
    }  else  {
    //insert new node:
      allocNode(hash,  key,  data,  basketID,  cmp);
    }
  }  //  insertNode

  void  find_bug()  {
    for (unsigned short b  =  0;  b  <  _hash_baskets;   ++b)  {
      TONode  *cur  =  &(baskets[b]);
      if (cur->fwdPtrs[0]) {
        cur  =  cur->fwdPtrs[0];
      }  else  {
        continue;
      }
      while (cur)  {
        assert(cur->hash % _hash_baskets  ==  b);
        for (unsigned char  h  =  0;  h  <  SKIPHEIGHT;  ++h)  {
          if (cur->fwdPtrs[h]) {
            assert(cur->fwdPtrs[h]->hash % _hash_baskets  ==  b);
          }
        }
        cur  =  cur->fwdPtrs[0];
      }
    }
  }  // find_bug

 private:
  const unsigned int  _capacity;
  const unsigned short  _hash_baskets;
  const unsigned short  leafSize;
  unsigned int  _size;
  TONode  *baskets;
  TONode  *updatePathOut[SKIPHEIGHT];
  unsigned long long  hashOut  =  0;

    //Allocations:    
  TONode  *curLeaf;
  TONode  **curLeaf_NextPtr;
  TONode  **headLeaf;
  unsigned int  leafAllocCounter;

    //Deallocations:    
  TDeletDataFunc  _f_delData;

    //Rating queue:    
  TONode  headNode;

    //Landscapes    
  unsigned char  landscape_h[256];
  unsigned char  *land_h_p;
  unsigned char  landscape_l[256];
  unsigned char  *land_l_p;

  void  init()  {
    const size_t size1  =  _hash_baskets  *  sizeof(unsigned char);
    land_h_p  =  (unsigned char *) malloc(size1);
    land_l_p  =  (unsigned char *) malloc(size1);
    memset(land_h_p,  0,  size1);
    memset(land_l_p,  0,  size1);
    landscape_h[0]  =  4;
    landscape_l[0]  =  1;
    const unsigned short  lvl2jump  =  (sqrt(_hash_baskets));
    const unsigned short  lvl1jump  =  (sqrt(lvl2jump));
    int delLvl2  =  lvl2jump  +  1;
    int delLvl1  =  lvl1jump  +  1;
    for (int  i  =  1;  i  <  255;  ++i)  {
      if  (i % delLvl2  >=  lvl2jump)  {
        landscape_h[i]  =  4;
        landscape_l[i]  =  0;
      } else if (i % delLvl1  >=  lvl1jump)  {
        landscape_h[i]  =  3;
        landscape_l[i]  =  1;
      }  else  {
        landscape_h[i]  =  2;
        landscape_l[i]  =  0;
      }
    }
    landscape_l[255]  =  1;
    landscape_h[255]  =  4;

        //init baskets:        
    const size_t  size2  = _hash_baskets  *  sizeof(TONode);
    baskets  =  (TONode *)malloc(size2);
    memset(baskets,  0,  size2);
    for  (int  i  =  0;  i < _hash_baskets;  ++i)  {
      baskets[i].curHeight  =  4;
    }

        //init allocations:        
    const size_t  size3  =  sizeof(TONode * )  +  leafSize * sizeof(TONode);
    headLeaf  =  curLeaf_NextPtr  =  (TONode **)malloc(size3);
    memset(curLeaf_NextPtr,  0,  size3);
    curLeaf  =  (TONode * )(curLeaf_NextPtr  +  1);
    *curLeaf_NextPtr  =  nullptr;
    leafAllocCounter  =  0;
    _size  =  0;

        //init rating queue:                
    memset(&headNode,  0,  sizeof(TONode));
    headNode.mostUseful  =  &headNode;
    headNode.leastUseful  =  &headNode;
  } //init

  void  clear()  {
    deleteLeaf(headLeaf);
    headLeaf  =  nullptr;
    curLeaf  =  nullptr;
    curLeaf_NextPtr  =  nullptr;
    leafAllocCounter  =  0;
    _size  =  0;
    free(baskets);
    free(land_h_p);
    free(land_l_p);
  }

  void  deleteLeaf(TONode  **ptr)  {
    if (ptr)  {
      if (*ptr)  {  deleteLeaf((TONode **) *ptr);  }
      TONode  *node  =  (TONode *)(ptr  +  1);
      if (curLeaf  ==  node)  {
        clearNodes(curLeaf,  leafAllocCounter);
        curLeaf  =  nullptr;
        leafAllocCounter  =  0;
      }  else  {
        clearNodes(node,  leafSize);
      }
      free(ptr);
    }
  }

  void  allocNode(const unsigned long long  hash,  TKey const  *key,
      const char  *data,  const unsigned short  basketID,  int  cmp)  {
    TONode  *re  =  nullptr;
    TONode  *prevHead  =  updatePathOut[0];
    if (_capacity  >  _size)  {
      if (leafSize  ==  leafAllocCounter)  {
        *curLeaf_NextPtr  =  (TONode *)malloc(sizeof(TONode * )  +  leafSize * sizeof(TONode));
        if  (*curLeaf_NextPtr)  {
                    //if alloc success                    
          curLeaf_NextPtr  =  (TONode **)(*curLeaf_NextPtr);
          curLeaf  =  (TONode * )(curLeaf_NextPtr  +  1);
          *curLeaf_NextPtr  =  nullptr;
          leafAllocCounter  =  0;
        }
      }
      if (leafSize  >  leafAllocCounter)  {
        re  =  curLeaf  +  leafAllocCounter;
        ++_size;
        ++leafAllocCounter;
      }
    }

    if (!re)  {
            //reuse an older node            
      re  =  headNode.leastUseful;
      if (re->key->keyArray[0]==138286) {
          std::cout<<"\n";
      }
      headNode.leastUseful  =  re->mostUseful;
      re->mostUseful->leastUseful  =  &headNode;
            //check if this is head of basket:            
      if (hash  ==  re->hash)  {
        if (re  !=  prevHead)  {
          delInSameBasket(re);
          prevHead  =  updatePathOut[0];
        } //else will replace at place
      } else if (basketID  ==  re->hash % _hash_baskets)  {
        delInSameBasket(re);
        prevHead  =  updatePathOut[0];
      }  else  {
        delInOtherBacket(re);
      }
      _f_delData(re->data);
    }  //  if (!re)

    memset(re,  0,  sizeof(TONode));
        //New leader = new:        
    re->mostUseful  =  &headNode;
    re->leastUseful  =  headNode.mostUseful;
    headNode.mostUseful->mostUseful  =  re;
    if (&headNode  ==  headNode.leastUseful)  {
            //The first became last too:            
      headNode.leastUseful  =  re;
    }
    headNode.mostUseful  =  re;
    re->hash  =  hash;
    if (cmp  >  0)  {
            //using update path            
      re->key  =  key;
      re->data  =  data;
      re->curHeight  =  (3  ==  cmp)?  landscape_h[(land_h_p[basketID])++]
          :  landscape_l[(land_l_p[basketID])++];
      int  i  =  0;
      while (i  <=  re->curHeight)  {
        re->fwdPtrs[i]  =  updatePathOut[i]->fwdPtrs[i];
        updatePathOut[i]->fwdPtrs[i]  =  re;
        ++i;
      }
//      while (i  <  SKIPHEIGHT)  { memset(re,  0,  sizeof(TONode));
//        re->fwdPtrs[i]  =  nullptr;
//        ++i;
//      }
    }  else  {
      //replace at place
      if  (re  ==  prevHead)  {
        re->key  =  key;
        re->data  =  data;
      }  else  {
        re->key  =  prevHead->key;
        re->data  =  prevHead->data;
        re->curHeight  =  landscape_l[(land_l_p[basketID])++];
        re->fwdPtrs[0]  =  prevHead->fwdPtrs[0];
        prevHead->fwdPtrs[0]  =  re;
        if (1  ==  re->curHeight)  {
          re->fwdPtrs[1]  =  prevHead->fwdPtrs[1];
          prevHead->fwdPtrs[1]  =  re;
        }
        prevHead->key  =  key;
        prevHead->data  =  data;
      }
    }
    return;
  }  //  allocNode


  TONode * find(TKey const  *key)  {
    const unsigned long long hash  =  key->hash();
    const unsigned short basketID  =  hash % _hash_baskets;
    TONode * cur  =  &(baskets[basketID]);
    int h  =  4;//cur->curHeight;
    while (h>1)  {
      while (cur->fwdPtrs[h]  &&  hash  >  cur->fwdPtrs[h]->hash)  {
        cur  =  cur->fwdPtrs[h]; //step on it
      }
      --h;
    }  //  while

//same key jumps        
    if (cur->fwdPtrs[2] && hash  ==  cur->fwdPtrs[2]->hash)  {
            //step on same hash:             
      cur  =  cur->fwdPtrs[2];
            //same hash found, next search for same key                        
      int cmp  =  key->cmp(cur->key);
      if (cmp  <  0)  {
        return nullptr; //nothing bigger with same hash
      } else if (0  ==  cmp)  {
        return cur;
      }

      while (cur->fwdPtrs[1] && hash  ==  cur->fwdPtrs[1]->hash)  {
        cmp  =  key->cmp(cur->fwdPtrs[1]->key);
        if (cmp  <  0)  {
          //found who bigger
          break;
        }
        if (0  ==  cmp)  {
          return cur->fwdPtrs[1];
        }
        cur  =  cur->fwdPtrs[1]; //step on it
      }

      while (cur->fwdPtrs[0] && hash  ==  cur->fwdPtrs[0]->hash)  {
        cmp  =  key->cmp(cur->fwdPtrs[0]->key);
        if (cmp  <  0)  {
          //found who bigger
          return nullptr;
        }
        if (0  ==  cmp)  {
          return cur->fwdPtrs[0];
        }
        cur  =  cur->fwdPtrs[0]; //step on it
      }
    }
    return nullptr;
  }  //  find


    /*
     * return:
     * 0 ==node with equal key if found
     * 3 == updatePath to insert node with new hash
     * 1 == updatePath to insert node with same hash
     * -N == node with same hash but bigger, to replace at place
   */
    int  setll(unsigned long long  hash,
        TKey const  *key,  const unsigned short basketID)  {
      TONode  *cur  =  &(baskets[basketID]);
      int  h  =  4;  //cur->curHeight;
      hashOut  =  hash;
      while ( h>1 )  {
        while (cur->fwdPtrs[h] && hash  >  cur->fwdPtrs[h]->hash)  {
          cur  =  cur->fwdPtrs[h];  //step on it
        }
            //Update path always point to node that point to bigger or equal one            
        updatePathOut[h] = cur;
        --h;
      } //while

//same key jumps               
      if (cur->fwdPtrs[2] && hash  ==  cur->fwdPtrs[2]->hash)  {
            //step on same hash:             
        cur  =  cur->fwdPtrs[2];
            //same hash found, next search for same key                        
        int  cmp  =  key->cmp(cur->key);
        if (cmp  <=  0)  {          
          updatePathOut[0]  =  updatePathOut[1]  =  cur;
          return cmp; //must replace hash head in place
        }

        while (cur->fwdPtrs[1] && hash  ==  cur->fwdPtrs[1]->hash)  {
          cmp  =  key->cmp(cur->fwdPtrs[1]->key);
          if (cmp  <  0)  {
            //found who bigger
            break;
          }
          if (0  ==  cmp)  {
            //updatePathOut[0]  =  cur->fwdPtrs[1];
            updatePathOut[0]  =  updatePathOut[1]  =  cur->fwdPtrs[1];
            return 0; //must replace
          }
          cur  =  cur->fwdPtrs[1]; //step on it
        }
        updatePathOut[1]  =  cur;

        while (cur->fwdPtrs[0]  &&  hash == cur->fwdPtrs[0]->hash)  {
          cmp  =  key->cmp(cur->fwdPtrs[0]->key);
          if (cmp  <  0)  {
            //found who bigger
            break;
          }
          if (0  ==  cmp)  {
            updatePathOut[0]  =  cur->fwdPtrs[0];
            return 0; //must replace
          }
          cur  =  cur->fwdPtrs[0]; //step on it
        }
        updatePathOut[0]  =  cur;
        return 1; //base alhorithm==insert on updatePath
      }  else  {
        //scroll same hash:
        hash  =  cur->hash;
        while (cur->fwdPtrs[1]  &&  hash == cur->fwdPtrs[1]->hash)  {
          cur  =  cur->fwdPtrs[1];
        }
        updatePathOut[1]  =  cur;
        while (cur->fwdPtrs[0]  &&  hash == cur->fwdPtrs[0]->hash)  {
          cur  =  cur->fwdPtrs[0];
        }
        updatePathOut[0]  =  cur;

        return 3; //base alhorithm==insert on updatePath
      }
      return 3;
    }

    void  clearNode(TONode  &node)  {
      if (node.data)  {
        _f_delData(node.data);
      }
    }

    void  clearNodes(TONode  *nodes,  unsigned int  toClear)  {
      for (unsigned int i=0; i  <  toClear;  ++i)  {
        clearNode(nodes[i]);
      }
    }

    void  delInOtherBacket(TONode  *nodeToDel)  {
      const unsigned long long  hash  =  nodeToDel->hash;
      TONode  *cur  =  &(baskets[hash % _hash_baskets]);
      TONode  *updatePath[SKIPHEIGHT];
      int  h  =  4;  //cur->curHeight;
//same hash jumps        
      while ( h>1 )  {
        updatePath[h]  =  cur;
        while (cur->fwdPtrs[h] && hash  >  cur->fwdPtrs[h]->hash)  {
          updatePath[h]  =  cur; //start from head that always smaller
          cur  =  cur->fwdPtrs[h]; //step on it
        }
        //if (cur->fwdPtrs[h]  &&  hash  ==  cur->fwdPtrs[h]->hash)  {
        if (cur->fwdPtrs[h]  &&  hash <= cur->fwdPtrs[h]->hash)  {
          updatePath[h]  =  cur;
        }
        --h;
      } //while

        //same key jumps        
      while (cur->fwdPtrs[1] && hash  ==  cur->fwdPtrs[1]->hash)  {
        updatePath[1]  =  cur; //need in case of null==cur->fwdPtrs[1]
        if (nodeToDel->key->cmp(cur->fwdPtrs[1]->key)  <=  0)  {
          break;
        }
        cur  =  cur->fwdPtrs[1]; //step on it
      }

      while (cur->fwdPtrs[0]  !=  nodeToDel)  {
        cur  =  cur->fwdPtrs[0]; //step on it
      }
      updatePath[0]  =  cur;

      if (updatePath[2]->fwdPtrs[2]  ==  nodeToDel)  {
            //This is the head of hash queue:            
        cur  =  nodeToDel->fwdPtrs[0]; //new head
        if (cur  &&  cur->curHeight  <  nodeToDel->curHeight)  {
          for (h = nodeToDel->curHeight;  h  >  cur->curHeight;  --h)  {
            cur->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];  // new head see what nodeToDel see now
            nodeToDel->fwdPtrs[h]  =  cur; // that will be used next for updatePath[h]->fwdPtrs[h]
          }
          cur->curHeight  =  nodeToDel->curHeight;
        }
      }  //  if (updatePath[2]->fwdPtrs[2]  == 3

      for (h  =  nodeToDel->curHeight;  h  >=  0;  --h)  {
        updatePath[h]->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];
      }
      return;
    } //delInOtherBasket

    void delInSameBasket(TONode  *nodeToDel)  {
      for (int  h  =  4;  h  >=  0;  --h)  {
        if (nodeToDel  ==  updatePathOut[h])  {
          //worst case - must do repath
          delInSameBasketPathOut(nodeToDel, h);
          return;
        }
        if (nodeToDel  ==  updatePathOut[h]->fwdPtrs[h])  {
          //best case - know where and how to update
          delInSameBasketSuperFast(nodeToDel, h);
          return;
        }
      }
        //not on path == path not affected         
      delInSameBasketFast(nodeToDel);
      return;
    }

    void delInSameBasketPathOut(TONode  *nodeToDel,  int  top_h)  {
        //Node to del in updatePathOut[h], need path to it for update        
      const unsigned long long  hash  =  nodeToDel->hash;
      TONode  *updatePath[SKIPHEIGHT];
      int  h  =  4;
      TONode  *cur  =  &(baskets[hash % _hash_baskets]);
      while (h  >  top_h)  {
        cur=updatePath[h]  =  updatePathOut[h];
        --h;
      }
      while (h  >  1)  {
        updatePath[h]  =  cur;
        while (hash  >  cur->fwdPtrs[h]->hash)  {
          updatePath[h]  =  cur;
          cur  =  cur->fwdPtrs[h];
        }
        if (cur->fwdPtrs[h]  &&  hash <= cur->fwdPtrs[h]->hash)   {
          updatePath[h]  =  cur;
        }
        --h;
      } //while

        //same key jumps        
      while (cur->fwdPtrs[1] && hash  ==  cur->fwdPtrs[1]->hash)  {
        updatePath[1]  =  cur;
        if (nodeToDel->key->cmp(cur->fwdPtrs[1]->key)  <=  0)  {
          break;
        }
        cur  =  cur->fwdPtrs[1]; //step on it
      }

      while (cur->fwdPtrs[0]  !=  nodeToDel)  {
        cur  =  cur->fwdPtrs[0]; //step on it
      }
      updatePath[0]  =  cur;

            //final update paths:                        
      if (updatePath[2]->fwdPtrs[2]  ==  nodeToDel)  {
        //This is the head of hash queue:
        cur  =  nodeToDel->fwdPtrs[0]; //new head
        if (cur  &&  cur->curHeight  <  nodeToDel->curHeight)  {
          for (h = nodeToDel->curHeight;  h  >  cur->curHeight;  --h)  {
            cur->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];  // new head see what nodeToDel see now
            nodeToDel->fwdPtrs[h]  =  cur; // that will be used next for updatePath[h]->fwdPtrs[h]
          }
          cur->curHeight  =  nodeToDel->curHeight;
        }
      }  //  if (updatePath[2]->fwdPtrs[2]  == 4

      for (h  =  top_h;  h  >=  0;  --h)  {
        updatePath[h]->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];
        //replace deleted node :
        if (nodeToDel  ==  updatePathOut[h])  {
          //updatePathOut[h]  =  updatePath[h];
          if (nodeToDel->fwdPtrs[h]
              && nodeToDel->fwdPtrs[h]->hash <= hashOut)  {
            updatePathOut[h]  =  nodeToDel->fwdPtrs[h];
          }  else  {
            updatePathOut[h]  =  updatePath[h];
          }
        }        
      }
      return;
    }//delInSameBasketPathOut

    void delInSameBasketSuperFast(TONode  *nodeToDel,  int  top_h)  {
        //need path before to update pointers        
      const unsigned long long  hash  =  nodeToDel->hash;
      TONode * updatePath[SKIPHEIGHT];
      updatePath[top_h]  =  updatePathOut[top_h];
      TONode  *cur  =  updatePathOut[top_h];
      int  h  =  top_h  -  1;
      while (h  >  1)  {
        if (cur->hash  <  updatePathOut[h]->hash)  {
          cur  =  updatePathOut[h];
        }
        updatePath[h]  =  cur;
        while (cur->fwdPtrs[h] && hash  >  cur->fwdPtrs[h]->hash)  {
          updatePath[h]  =  cur;
          cur  =  cur->fwdPtrs[h];
        }
        if (cur->fwdPtrs[h]  &&  hash <= cur->fwdPtrs[h]->hash)   {
          updatePath[h]  =  cur;
        }
        --h;
      } //while

        //same key jumps                
      while (cur->fwdPtrs[1] && hash  ==  cur->fwdPtrs[1]->hash)  {
        updatePath[1]  =  cur;
        if (nodeToDel->key->cmp(cur->fwdPtrs[1]->key)  <=  0)  {
          break;
        }
        cur  =  cur->fwdPtrs[1]; //step on it
      }

      while (cur->fwdPtrs[0]  !=  nodeToDel)  {
        cur  =  cur->fwdPtrs[0]; //step on it
      }
      updatePath[0]  =  cur;

      //final update paths:
      if (updatePath[2]->fwdPtrs[2]  ==  nodeToDel)  {
        //This is the head of hash queue:
        cur  =  nodeToDel->fwdPtrs[0]; //new head
        if (cur  &&  cur->curHeight  <  nodeToDel->curHeight)  {
          for (h = nodeToDel->curHeight;  h  >  cur->curHeight;  --h)  {
            cur->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];  // new head see what nodeToDel see now
            nodeToDel->fwdPtrs[h]  =  cur; // that will be used next for updatePath[h]->fwdPtrs[h]
          }
          cur->curHeight  =  nodeToDel->curHeight;
        }
      }  //  if (updatePath[2]->fwdPtrs[2]  == 1

      for (h  =  top_h;  h  >=  0;  --h)  {
        updatePath[h]->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];
      }
      return;
    }  //  delInSameBasketSuperFast

    void  delInSameBasketFast(TONode  *nodeToDel)  {
      const unsigned long long  hash  =  nodeToDel->hash;
      TONode * updatePath[SKIPHEIGHT];
      int  h  =  4;
      TONode  *cur  =  &(baskets[hash % _hash_baskets]);
      while (h  >  1)  {
        //Use all known info to narrow jump:
        if (updatePathOut[h]->hash <= hash
            &&  cur->hash < updatePathOut[h]->hash)  {
          cur  =  updatePathOut[h];
        }
        updatePath[h]  =  cur;
        while (cur->fwdPtrs[h] && hash  >  cur->fwdPtrs[h]->hash)  {
          updatePath[h]  =  cur;
          cur  =  cur->fwdPtrs[h];
        }
        if (cur->fwdPtrs[h]  &&  hash <= cur->fwdPtrs[h]->hash)   {
          updatePath[h]  =  cur;
        }
        --h;
      } //while

        //same key jumps        
      updatePath[1]  =  cur; //need in case of null==cur->fwdPtrs[1]
      while (cur->fwdPtrs[1] && hash  ==  cur->fwdPtrs[1]->hash)  {
        updatePath[1]  =  cur;
        if (nodeToDel->key->cmp(cur->fwdPtrs[1]->key)  <=  0)  {
          break;
        }
        cur  =  cur->fwdPtrs[1]; //step on it
      }

      while (cur->fwdPtrs[0]  !=  nodeToDel)  {
        cur  =  cur->fwdPtrs[0]; //step on it
      }
      updatePath[0]  =  cur;

      //final update paths:
      if (updatePath[2]->fwdPtrs[2]  ==  nodeToDel)  {
        //This is the head of hash queue:
        cur  =  nodeToDel->fwdPtrs[0]; //new head
//        if (cur)  {
//          for (h = nodeToDel->curHeight;  h  >  0;  --h)  {
//            if (h  >  cur->curHeight)  {
//              cur->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];
//            }
//            nodeToDel->fwdPtrs[h]  =  cur;
//          }
//          if (cur->curHeight  <  nodeToDel->curHeight)  {
//            cur->curHeight  =  nodeToDel->curHeight;
//          }
//        }
        if (cur  &&  cur->curHeight  <  nodeToDel->curHeight)  {
          for (h = nodeToDel->curHeight;  h  >  cur->curHeight;  --h)  {
            cur->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];  // new head see what nodeToDel see now
            nodeToDel->fwdPtrs[h]  =  cur; // that will be used next for updatePath[h]->fwdPtrs[h]
          }
          cur->curHeight  =  nodeToDel->curHeight;
        }
      }  //  if (updatePath[2]->fwdPtrs[2]  == 2

      for (h  =  nodeToDel->curHeight;  h  >=  0;  --h)  {
        updatePath[h]->fwdPtrs[h]  =  nodeToDel->fwdPtrs[h];
      }
      return;
    }  //  delInSameBasketFast

    void  toTopUsage(TONode  *node)  {
        //Exсlude:        
      if (node->mostUseful)  {
        node->mostUseful->leastUseful  =  node->leastUseful;
      }
      if (node->leastUseful)  {
        node->leastUseful->mostUseful  =  node->mostUseful;
      }

        //New leader:        
      node->mostUseful  =  &headNode;
      node->leastUseful  =  headNode.mostUseful;
      headNode.mostUseful->mostUseful  =  node;
      if (&headNode   ==  headNode.leastUseful)  {
        //The first became last too:
        headNode.leastUseful  =  node;
      }
      headNode.mostUseful  =  node;
    }
};

#endif // ONCACHE_H
