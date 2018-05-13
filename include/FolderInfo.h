#ifndef __FOLDERINFO_H__
#define __FOLDERINFO_H__
#include <string>
#include <set>

class FolderInfo
{
public:
    FolderInfo(const std::string &path);
    void add_media(std::string &name);
    void add_cue(std::string &name);
    int process(void);

    std::string root_path;
    std::set<std::string> filename_cue;
    std::set<std::string> filename_music;
};
#endif
