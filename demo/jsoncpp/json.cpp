#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <jsoncpp/json/json.h>

// 序列化
bool serialize(const Json::Value &val, std::string &body)
{
    // 先得有工厂类
    Json::StreamWriterBuilder swb;
    swb.settings_["emitUTF8"] = true; // 加上这句防止输出乱码
    // 通过工厂类创建StreamBuiler
    std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

    std::stringstream ss;
    int ret = sw->write(val, &ss);
    if (ret != 0)
    {
        return false;
    }
    body = ss.str();
    return true;
}

// 反序列化
bool unserialize(const std::string &body, Json::Value &val)
{
    // 先定义工厂类
    Json::CharReaderBuilder crb;
    crb.settings_["emitUTF8"] = true; // 禁止值转义为Unicode
    std::unique_ptr<Json::CharReader> cr(crb.newCharReader());

    std::string errs;
    bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), &val, &errs);
    if (ret == false)
    {
        std::cout << "json unserialize failed : " << errs << std::endl;
        return false;
    }
    return true;
}

int main()
{
    // 定义数据
    std::string name = "张三";
    int age = 20;
    std::string sex = "男";
    double socre[3] = {90.0, 85.5, 95.5};

    Json::Value hobby;
    hobby["书籍"] = "三体";
    hobby["运动"] = "乒乓球";

    // 填写Value对象
    Json::Value student;
    student["姓名"] = name;
    student["性别"] = sex;
    student["年龄"] = 20;
    student["分数"].append(socre[0]);
    student["分数"].append(socre[1]);
    student["分数"].append(socre[2]);
    student["爱好"] = hobby;

    std::string out;

    serialize(student, out);
    std::cout << out << std::endl;

    std::string str = u8R"({"姓名":"李四", "年龄":19,"成绩":[60, 70, 80]})";
    Json::Value stu;
    bool ret = unserialize(str, stu);
    if (ret == false)
    {
        return -1;
    }
    std::cout << "姓名: " << stu["姓名"] << std::endl;
    std::cout << "年龄: " << stu["年龄"] << std::endl;
    int sz = stu["成绩"].size();
    for (int i = 0; i < sz; i++)
    {
        std::cout << "成绩: " << stu["成绩"][i].asFloat() << std::endl;
    }
    return 0;
}