#include <dirent.h>
#include <errno.h>
#include "disk_info.h"
#include "FolderInfo.h"
#include <iostream>
#include "log.h"
#include "Processor.h"
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <unistd.h>

extern Logger logger;

Processor::Processor(const std::string &src_path, const std::string &dst_path) :
    src_path_(src_path), dst_path_(dst_path)
{
}

int Processor::process(void)
{
    return process_dir(src_path_, dst_path_);
}

int Processor::process_dir(const std::string &src_path, const std::string &dst_path)
{
    DIR *dir;
    struct dirent *ptr;
    FolderInfo folder_info(src_path);
    int ret = 0;

    dir = opendir(src_path.c_str());
    if (dir == nullptr)
    {
        logger(LEVEL_ERROR, "Dir open err: %s", src_path.c_str());
        return -EINVAL;
    }
    while ((ptr = readdir(dir)) != nullptr)
    {
        std::string filename(ptr->d_name);
        if (filename == "." || filename == "..")
            continue;

        //std::cout << "--name: " << ptr->d_name << std::endl;
        eFileType type = get_file_type(src_path + "/" + ptr->d_name);
        switch (type)
        {
            case eFileType_Media:
                folder_info.add_media(filename);
                break;
            case eFileType_Cue:
                folder_info.add_cue(filename);
                break;
            case eFileType_Folder:
                process_dir(src_path + "/" + ptr->d_name, dst_path);
                break;
            default:
                break;
        }
    }
    closedir(dir);

    if (folder_info.is_media_folder() == false)
    {
        return 0;
    }

    ret = make_clean_folder(dst_path + "/" + folder_info.root_path);
    if (ret != 0)
    {
        return ret;
    }

    /*
     * First Process Cue File.
     * The remain media files will be converted in whole
     */
    for (auto cue_index = folder_info.filename_cue.begin();
            cue_index != folder_info.filename_cue.end();
            cue_index++)
    {
        DiscInfo disc_info;
        std::string full_path = folder_info.root_path + "/" + *cue_index;

        //std::cout << "Processing " << full_path << std::endl;
        logger(LEVEL_INFO, "Processing: %s", full_path.c_str());

        disc_info.parse_file(full_path);

        convert_cue_files(dst_path + "/" + folder_info.root_path + "/" +
                (*cue_index).substr(0, (*cue_index).find_last_of('.')),
                disc_info, folder_info);
    }
    /*
     * Process remaining Media Files
     */
    for (auto i = folder_info.filename_music.begin(); i != folder_info.filename_music.end(); i++)
    {
        std::string src_file = folder_info.root_path + "/" + *i;
        convert_file(dst_path + "/" + folder_info.root_path + "/" +
                (*i).substr(0, (*i).find_last_of('.')) + ".m4a",
                    src_file);

    }
    return 0;
}

int Processor::is_folder_exist(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
        return 0;
    if ((sb.st_mode & S_IFMT) == S_IFDIR)
        return 1;
    else
    {
        logger(LEVEL_ERROR, "Not folder: %s", path.c_str());
        return -EINVAL;
    }
}

eFileType Processor::get_file_type(const std::string &path)
{
    struct stat sb;
    if (stat(path.c_str(), &sb) == -1)
    {
        logger(LEVEL_ERROR, "Stat Failed: %s", path.c_str());
        return eFileType_None;
    }
    switch (sb.st_mode & S_IFMT)
    {
        case S_IFDIR:
            return eFileType_Folder;
        case S_IFREG:
        {
            int index = path.find_last_of('.');
            std::string file_type = path.substr(index + 1, path.size() - index);
            if (file_type == "ape" || file_type == "flac" || file_type == "wav")
            {
                return eFileType_Media;
            }
            else if (file_type == "cue")
            {
                return eFileType_Cue;
            }
            else
            {
                return eFileType_UnSupported;
            }
        }
        default:
            return eFileType_Unknown;
    }
}

int Processor::clean_folder(const std::string &src_path)
{
    DIR *dir;
    struct dirent *ptr;
    int ret = 0;

    dir = opendir(src_path.c_str());
    if (dir == nullptr)
    {
        logger(LEVEL_ERROR, "Dir open err: %s", src_path.c_str());
        return -EINVAL;
    }
    while ((ptr = readdir(dir)) != nullptr)
    {
        std::string filename(ptr->d_name);
        if (filename == "." || filename == "..")
            continue;

        //std::cout << "--name: " << ptr->d_name << std::endl;
        filename = src_path + "/" + ptr->d_name;
        eFileType type = get_file_type(filename);
        if (type == eFileType_Folder)
        {
            ret = clean_folder(filename);
        }
        else
        {
            ret = remove(filename.c_str());
        }
        if (ret != 0)
        {
            logger(LEVEL_ERROR, "Failed remove: %s", filename.c_str());
            break;
        }
    }
out:
    closedir(dir);
    return ret;
}

int Processor::create_folder(const std::string &src_path)
{
    std::istringstream iss(src_path);
    std::string s, path_;
    int ret;

    while(getline(iss, s, '/' ))
    {
        if (s.size() == 0)
            continue;
        path_ += "/" + s;
        ret = is_folder_exist(path_);
        if (ret == 1)
            continue;
        else if (ret < 0)
        {
            logger(LEVEL_ERROR, "Invalid folder: %s", path_.c_str());
            return ret;
        }
        else
        {
            ret = mkdir(path_.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            if (ret != 0)
            {
                logger(LEVEL_ERROR, "Failed create folder: %s", path_);
                return ret;
            }
        }
    }
    return 0;
}

int Processor::make_clean_folder(const std::string &path)
{
    struct stat sb;
    int ret = 0;

    if (stat(path.c_str(), &sb) == 0) //already exists
    {
        ret = clean_folder(path);
        if (ret)
            return ret;
    }
    ret = create_folder(path);
    return ret;
}

int Processor::convert_file(const std::string &dst_file, const std::string &src_file)
{
//    std::cout <<"src: " << src_file << std::endl;
//    std::cout <<"dst: " << dst_file << std::endl;
    int ret = 0;
    int length_filename = src_file.size() + dst_file.size();

    char *cmd = nullptr;
    try {
        cmd = new char[length_filename + 1024];

        sprintf(cmd, "ffmpeg -i %s -acodec alac %s",
                src_file.c_str(),
                dst_file.c_str());
        logger(LEVEL_INFO, "cmd: %s", cmd);
        system(cmd);
    }
    catch (std::exception &e)
    {
        ret = -ENOMEM;
    }
    if (cmd != nullptr)
        delete[] cmd;
    return ret;
}

int Processor::convert_file(std::string &dst_file, std::string &src_file,
        DiscInfo &disc_info, int index)
{
    TrackInfo *track_info = &disc_info.tracks[index];
    if (track_info->start_time < 0)
    {
        return -EINVAL;
    }
    /*
     * FIXME!
     * It should have replace list, or
     * will use lib-ffmpeg instead of currentshell command
     */
    src_file = std::regex_replace(src_file, std::regex(" "), "\\ ");
    dst_file = std::regex_replace(dst_file, std::regex(" "), "\\ ");
    dst_file = std::regex_replace(dst_file, std::regex("\\("), "\\(");
    dst_file = std::regex_replace(dst_file, std::regex("\\)"), "\\)");
    //std::cout << "src: " << src_file << std::endl;
    //std::cout << "dst: " << dst_file << std::endl;

    /*
     * metadata:
     * - track titile
     * - track performer
     * - album
     * - track
     */
    int length_metadata = track_info->title.size() +
        track_info->performer.size() + track_info->title.size();
    int length_filename = src_file.size() + dst_file.size();

    char *meta_buf = nullptr;
    char *cmd = nullptr;
    int ret = 0;
    try {
        meta_buf = new char[length_metadata + 128];
        cmd = new char[length_filename + 1024];

        sprintf(meta_buf, "-metadata album=\"%s\" -metadata artist=\"%s\" "
                "-metadata title=\"%s\" -metadata track=%d",
                disc_info.title.c_str(),
                track_info->performer.c_str(),
                track_info->title.c_str(),
                index);
        if (track_info->duration > 0)
        {
            sprintf(cmd, "ffmpeg -ss %d -i %s -t %d %s -acodec alac %s",
                    track_info->start_time,
                    src_file.c_str(),
                    track_info->duration,
                    meta_buf,
                    dst_file.c_str());
        }
        else
        {
            sprintf(cmd, "ffmpeg -ss %d -i %s %s -acodec alac %s",
                    track_info->start_time,
                    src_file.c_str(),
                    meta_buf,
                    dst_file.c_str());
        }
        logger(LEVEL_INFO, "cmd: %s", cmd);
        system(cmd);
    }
    catch (std::exception &e)
    {
        ret = -ENOMEM;
    }
    if (meta_buf != nullptr)
        delete[] meta_buf;
    if (cmd != nullptr)
        delete[] cmd;
    return ret;
}

int Processor::convert_cue_files(const std::string &work_root,
        DiscInfo &disk_info,
        FolderInfo &folder_info)
{
    std::set<std::string> files_used;
    int ret;

    //std::cout << "Preparing Folder: " << work_root << std::endl;
    ret = make_clean_folder(work_root);
    if (ret != 0)
    {
        return ret;
    }
    //print_cd_info(&disk_info);
    for (int i = 0; i < disk_info.tracks.size(); i++)
    {
        TrackInfo *track_info = &disk_info.tracks[i];
        std::string src_file = folder_info.root_path + "/" + track_info->file;
        std::string title = std::regex_replace(track_info->title, std::regex("/"), "-");
        std::string dest_file = work_root + "/" + std::to_string(i + 1) + "_" + title + ".m4a";
        //std::cout << "src: " << src_file << std::endl;
        //std::cout << "dst: " << dest_file << std::endl;
        //TODO: Check file existen, and start convert
        convert_file(dest_file, src_file, disk_info, i);
        files_used.insert(track_info->file);
    }
    /*
     * remove used files
     * This is for processing individual media files which does not contained
     * in the cue files
     */
    for (auto i = files_used.begin(); i!= files_used.end(); i++)
    {
        folder_info.filename_music.erase(*i);
    }
}

