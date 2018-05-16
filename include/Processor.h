#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__
#include "disk_info.h"
#include "FolderInfo.h"
#include <string>

class Processor
{
public:
    Processor(const std::string &src_path, const std::string &dst_path);
    int process(void);
private:
    std::string src_path_;
    std::string dst_path_;

    int process_dir(const std::string &src_path, const std::string &dst_path);
    int is_folder_exist(const std::string &path);
    eFileType get_file_type(const std::string &path);
    int clean_folder(const std::string &src_path);
    int create_folder(const std::string &src_path);
    int make_clean_folder(const std::string &path);
    int convert_file(const std::string &dst_file, const std::string &src_file);
    int convert_file(std::string &dst_file, std::string &src_file,
        DiscInfo &disc_info, int index);
    int convert_cue_files(const std::string &work_root,
        DiscInfo &disk_info,
        FolderInfo &folder_info);
};
#endif
