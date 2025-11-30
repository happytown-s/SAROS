/*
  ==============================================================================

    TrackUtils.h
    Created: 27 Oct 2025 6:31:55pm
    Author:  mt sh

  ==============================================================================
*/

#pragma once
#include <map>

template <typename TrackType,typename Predicate>
bool anyTrackSatisfies(const std::map<int, TrackType>& tracks, Predicate pred)
{
	for (const auto& [id, data] : tracks)
	{
		if (pred(data))
			return true;
	}
	return false;

}

template <typename TrackType,typename Predicate>
bool allTracksSatisfy(const std::map<int, TrackType>& tracks, Predicate pred)
{
	for (const auto& [id, data] : tracks)
	{
		if (!pred(data))
			return false;
	}
	return true;
}
