#include "FolderInfo.h"

FolderInfo::FolderInfo(const std::string &path) : root_path(path)
{
}

void FolderInfo::add_media(std::string &name)
{
    filename_music.push_back(name);
}

void FolderInfo::add_cue(std::string &name)
{
    filename_cue.push_back(name);
}
