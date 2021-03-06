#ifndef __FOLDERINFO_H__
#define __FOLDERINFO_H__
#include <string>
#include <set>

typedef enum
{
    eFileType_None,
    eFileType_Folder,
    eFileType_Media,
    eFileType_Cue,
    eFileType_Unknown,
    eFileType_UnSupported,
}eFileType;

class FolderInfo
{
public:
    FolderInfo(const std::string &path);
    void add_media(std::string &name);
    void add_cue(std::string &name);
    bool is_media_folder(void);

    std::string root_path;
    std::set<std::string> filename_cue;
    std::set<std::string> filename_music;
};
#endif
