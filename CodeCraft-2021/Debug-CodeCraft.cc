#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

using namespace std;

class Parameter{
	public:
		Parameter(int a1, int a2, int a3, int a4, int t)
			:i(a1), j(a2), k1(a3), k2(a4),
			totalCost(t){}
		~Parameter(){}
	public:
		double i;
		double j;
		double k1;
		double k2;
		int totalCost;
};

bool cmp(const Parameter &lhs, const Parameter &rhs){
	return (lhs.totalCost < rhs.totalCost);
}

int main(){
	vector<Parameter> yearAndCpu;
	//for(double i=1; i<=3; i+=0.2){
		//for(double j=0.3; j<=0.7; j+=0.1){
			for(double k1=1.8; k1<=2.3; k1+=0.1){
				for(double k2=1.8; k2<=2.3; k2+=0.1){
					double i = 1.8;
					double j = 0.4;
					yearAndCpu.emplace_back(i, j, k1, k2, 0);
					string exe_file1("./CodeCraft-2021 0 ");
					string exe_file2("./CodeCraft-2021 1 ");
					exe_file1 += to_string(i) + " " + to_string(j) + " ";
					exe_file2 += to_string(i) + " " + to_string(j) + " ";
					exe_file1 += to_string(k1) + " " + to_string(k2) + " 1000";
					exe_file2 += to_string(k1) + " " + to_string(k2) + " 1000";
					//cout << exe_file1 << endl;
					//cout << exe_file2 << endl;
					FILE* f1 = popen(exe_file1.c_str(), "r");
					FILE* f2 = popen(exe_file2.c_str(), "r");
					int cost1, cost2;
					fscanf(f1, "%d", &cost1);
					fscanf(f2, "%d", &cost2);
					pclose(f1);
					pclose(f2);
					auto it = yearAndCpu.end();
					--it;
					it->totalCost = cost1 + cost2;
					printf("i:%lf j:%lf k1:%lf k2:%lf", i, j, k1, k2);
					printf("\t%d\t%d\t%d\n", it->totalCost, cost1, cost2);

				}
			}
		//}
	//}
	auto it = min_element(yearAndCpu.begin(), yearAndCpu.end(), cmp);
	cout << endl;
	printf("i:%lf j:%lf k1:%lf k2:%lf", it->i, it->j, it->k1, it->k2);
	printf("\t%d\n", it->totalCost);
	return 0;
}
