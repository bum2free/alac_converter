#include "FolderInfo.h"

FolderInfo::FolderInfo(const std::string &path) : root_path(path)
{
}

void FolderInfo::add_media(std::string &name)
{
    filename_music.insert(name);
}

void FolderInfo::add_cue(std::string &name)
{
    filename_cue.insert(name);
}

bool FolderInfo::is_media_folder(void)
{
    if (filename_cue.size() == 0 && filename_music.size() == 0)
    {
        return false;
    }
    /*
    std::cout << "Folder: " << info.root_path << std::endl;
    std::cout << "\r - Num of cues: " << info.filename_cue.size() << std::endl;
    std::cout << "\r - Num of medias: " << info.filename_music.size() << std::endl;
    */
    return true;
}
