#include "json.hpp"
using json = nlohmann::json;

#include<iostream>
#include<string>
#include<map>
#include<vector>
using namespace std;

void test1(){
    json js;
    js["wth"] = "1999-03-07";
    js["vv"] = "1999-05-02";
    string ss = js.dump();
    cout << ss.c_str() << endl;
    // cout << js << endl;
}
int main(){
    test1();
}