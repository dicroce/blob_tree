// blob_tree
// MIT License
// 
// Copyright (c) 2018 Tony Di Croce
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef _blob_tree_h
#define _blob_tree_h

#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <string.h>
#include <arpa/inet.h>

namespace dicroce
{

class blob_tree
{
public:
    enum node_type
    {
        NT_OBJECT,
        NT_ARRAY,
        NT_LEAF
    };

    static std::vector<uint8_t> serialize(const blob_tree& rt, uint32_t version)
    {
        auto size = sizeof(uint32_t) + _sizeof_treeb(rt);
        std::vector<uint8_t> buffer(size);
        uint8_t* p = &buffer[0];
        uint32_t word = htonl(version);
        *(uint32_t*)p = word;
        p+=sizeof(uint32_t);
        // &buffer[size] is our sentinel (1 past the end)
        _write_treeb(rt, p, &buffer[size]);
        return buffer;
    }

    static blob_tree deserialize(const uint8_t* p, size_t size, uint32_t& version)
    {
        auto end = p + size;
        uint32_t word = *(uint32_t*)p;
        p+=sizeof(uint32_t);
        version = ntohl(word);
        blob_tree obj; 
        _read_treeb(p, end, obj);
        return obj;
    }

    blob_tree& operator[](const std::string& key)
    {
        if(!_childrenByIndex.empty())
            throw std::runtime_error("blob_tree node cannot be both an array and an object.");
        return _children[key];
    }

    blob_tree& operator[](size_t index)
    {
        if(!_children.empty())
            throw std::runtime_error("blob_tree node cannot be both an object and an array.");
        if(_childrenByIndex.size() < (index+1))
            _childrenByIndex.resize(index+1);
        return _childrenByIndex[index];
    }

    size_t size()
    {
        if(!_children.empty())
            throw std::runtime_error("blob_tree node cannot be both an object and an array.");
        return _childrenByIndex.size();
    }

    blob_tree& operator=(const std::pair<size_t, const uint8_t*>& payload)
    {
        _payload = payload;
        return *this;
    }

    inline std::pair<size_t, const uint8_t*> get() const
    {
        return _payload;
    }

private:
    static size_t _sizeof_treeb(const blob_tree& rt)
    {
        size_t sum = 1 + sizeof(uint32_t); // type & num children

        if(!rt._children.empty())
        {
            for(auto& cp : rt._children)
                sum += sizeof(uint16_t) + cp.first.length() + _sizeof_treeb(cp.second);
        }
        else if(!rt._childrenByIndex.empty())
        {
            for(auto& c : rt._childrenByIndex)
                sum += _sizeof_treeb(c);
        }
        else sum += sizeof(uint32_t) + rt._payload.first;

        return sum;
    }
    static size_t _write_treeb(const blob_tree& rt, uint8_t* p, uint8_t* end)
    {
        uint8_t* fp = p;

        if(_bytes_left(p, end) < 5)
            throw std::runtime_error("Buffer too small to serialize blob_tree.");

        uint8_t type = (!rt._children.empty())?NT_OBJECT:(!rt._childrenByIndex.empty())?NT_ARRAY:NT_LEAF;
        *p = type;
        ++p;

        if(type == NT_OBJECT || type == NT_ARRAY)
        {
            uint32_t numChildren = (type==NT_OBJECT)?rt._children.size():rt._childrenByIndex.size();
            uint32_t word = htonl(numChildren);
            *(uint32_t*)p = word;
            p+=sizeof(uint32_t);

            if(type==NT_OBJECT)
            {
                for(auto& cp : rt._children)
                {
                    if(_bytes_left(p, end) < sizeof(uint16_t))
                        throw std::runtime_error("Buffer too small to serialize blob_tree.");
                    
                    uint16_t nameSize = cp.first.length();
                    uint16_t shortVal = htons(nameSize);
                    *(uint16_t*)p = shortVal;
                    p+=sizeof(uint16_t);

                    if(_bytes_left(p, end) < nameSize)
                        throw std::runtime_error("Buffer too small to serialize blob_tree.");

                    memcpy(p, cp.first.c_str(), nameSize);
                    p+=nameSize;

                    p+=_write_treeb(cp.second, p, end);
                }
            }
            else
            {
                for(auto& c : rt._childrenByIndex)
                    p+=_write_treeb(c, p, end);
            }
        }
        else
        {
            if(_bytes_left(p, end) < sizeof(uint32_t))
                throw std::runtime_error("Buffer too small to serialize blob_tree.");

            uint32_t payloadSize = rt._payload.first;
            uint32_t word = htonl(payloadSize);
            *(uint32_t*)p = word;
            p+=sizeof(uint32_t);

            if(_bytes_left(p, end) < payloadSize)
                throw std::runtime_error("Buffer too small to serialize blob_tree.");

            memcpy(p, rt._payload.second, payloadSize);
            p+=payloadSize;
        }

        return p - fp;
    }
    static size_t _read_treeb(const uint8_t* p, const uint8_t* end, blob_tree& rt)
    {
        if(_bytes_left(p, end) < 5)
            throw std::runtime_error("Buffer too small to deserialize blob_tree.");

        const uint8_t* fp = p;
        uint8_t type = *p;
        ++p;

        if(type == NT_OBJECT || type == NT_ARRAY)
        {
            uint32_t word = *(uint32_t*)p;
            p+=sizeof(uint32_t);
            uint32_t numChildren = ntohl(word);

            if(type == NT_OBJECT)
            {
                for(size_t i = 0; i < numChildren; ++i)
                {
                    if(_bytes_left(p, end) < sizeof(uint16_t))
                        throw std::runtime_error("Buffer too small to deserialize blob_tree.");

                    uint16_t shortVal = *(uint16_t*)p;
                    p+=sizeof(uint16_t);
                    uint16_t nameLen = ntohs(shortVal);

                    if(_bytes_left(p, end) < nameLen)
                        throw std::runtime_error("Buffer too small to deserialize blob_tree.");

                    std::string name((char*)p, nameLen);
                    p+=nameLen;
                    blob_tree childObj;
                    p+=_read_treeb(p, end, childObj);
                    rt._children[name] = childObj;
                }
            }
            else
            {
                rt._childrenByIndex.resize(numChildren);
                for(size_t i = 0; i < numChildren; ++i)
                    p+=_read_treeb(p, end, rt._childrenByIndex[i]);
            }
        }
        else
        {
            if(_bytes_left(p, end) < sizeof(uint32_t))
                throw std::runtime_error("Buffer too small to deserialize blob_tree.");

            uint32_t word = *(uint32_t*)p;
            p+=sizeof(uint32_t);
            uint32_t payloadSize = ntohl(word);

            if(_bytes_left(p, end) < payloadSize)
                throw std::runtime_error("Buffer too small to deserialize blob_tree.");

            rt._payload = std::make_pair((size_t)payloadSize, p);
            p+=payloadSize;
        }

        return p - fp;
    }
    inline static size_t _bytes_left(const uint8_t* p, const uint8_t* end) { return end - p; }

    std::map<std::string, blob_tree> _children;
    std::vector<blob_tree> _childrenByIndex;
    std::pair<size_t, const uint8_t*> _payload;
};

}

#endif
