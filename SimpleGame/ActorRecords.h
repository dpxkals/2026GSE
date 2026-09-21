#pragma once

#include "GameplayTypes.h"
#include <algorithm>
#include <vector>
#include <map>

template <class T> std::uint64_t RecordId(const T& record)
{
    return record.id;
}

inline std::uint64_t RecordId(const Projectile& record)
{
    return record.runtimeId;
}

inline std::uint64_t RecordId(const FloatingNumber& record)
{
    return record.runtimeId;
}

template <class T> T* FindRecord(std::vector<T>& records, std::uint64_t id)
{
    auto it = std::find_if(records.begin(),
                           records.end(),
                           [id](const T& record)
                           {
                               return RecordId(record) == id;
                           });
    return it == records.end() ? nullptr : &*it;
}

template <class T> const T* FindRecord(const std::vector<T>& records, std::uint64_t id)
{
    auto it = std::find_if(records.begin(),
                           records.end(),
                           [id](const T& record)
                           {
                               return RecordId(record) == id;
                           });
    return it == records.end() ? nullptr : &*it;
}

template <class T> void RemoveRecord(std::vector<T>& records, std::uint64_t id)
{
    records.erase(std::remove_if(records.begin(),
                                 records.end(),
                                 [id](const T& record)
                                 {
                                     return RecordId(record) == id;
                                 }),
                  records.end());
}

template <class T>
T* FindIndexedRecord(std::vector<T>& records,
                     const std::map<std::uint64_t, size_t>& indices,
                     std::uint64_t id)
{
    auto found = indices.find(id);
    if (found != indices.end() && found->second < records.size() &&
        RecordId(records[found->second]) == id)
    {
        return &records[found->second];
    }
    return FindRecord(records, id);
}
