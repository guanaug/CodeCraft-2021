#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <queue>
#include <list>
#include <unordered_map>
#include <algorithm>

using namespace std;

double yearOfN = 2.0; //计算多少年的电费
double aOfCpu = 0.4;  //cpu所占权重
double threshold_cpu_memory = 2.3;
double threshold_memory_cpu = 1.7;
double max_ratio = 500;

double cpuWeight = 0.8;
double memWeight = 1.2;


struct node{	//表示NUMA节点
	int cpuSize;
	int memorySize;
};
bool IsMoreCpu(double cpu, double memory){
	if(0 == memory)	memory = 1;
	return (1.0*cpu/memory >= threshold_cpu_memory);
}
bool IsMoreMemory(double cpu, double memory){
	if(0 == cpu)	cpu = 1;
	return (1.0*memory/cpu >= threshold_memory_cpu);
}

// 物理机类型
class ServerNode{
	public:
		ServerNode(const string &s):nodeVec(2){
			auto begin = s.begin();
			int index1 = s.find('(', 0);
			int index2 = s.find(',', 0);
			type = string(begin+index1+1, begin+index2);

			int cpu, memory;
			sscanf(s.c_str()+index2+1, " %d, %d, %d, %d", &cpu, &memory, &costBuy, &costPerDay);
			nodeVec[0].cpuSize = nodeVec[1].cpuSize = cpu/2;
			nodeVec[0].memorySize = nodeVec[1].memorySize = memory/2;
		}
		~ServerNode(){}

	public:
		string type;
		vector<struct node> nodeVec; //0，1下标分别代表A，B节点
		int costBuy;
		int costPerDay;
};

// 虚拟机类型
class VirtualNode{
	public:
		VirtualNode(const string &s){
			auto begin = s.begin();
			int index1 = s.find('(', 0);
			int index2 = s.find(',', 0);
			type = string(begin+index1+1, begin+index2);

			sscanf(s.c_str()+index2+1, " %d, %d, %d", &cpuSize, &memorySize, &twoNode);
		}
		~VirtualNode(){}

	public:
		string type;
		int cpuSize;
		int memorySize;
		int twoNode;	//0单节点部署，1双节点部署
};

// 请求结构
class RequestNode{
	public:
		RequestNode(const string &s){
			auto begin = s.begin();
			int index1 = s.find('(', 0);
			int index2 = s.find(',', 0);
			operation = string(begin+index1+1, begin+index2);
			string s2(begin+index2+2, s.end());
			if( operation == "add" ){
				int index3 = s2.find(',', 0);
				type = string(s2.begin(), s2.begin()+index3);
				istringstream ss(string(s2.begin()+index3+1, s2.end()-1));
				ss >> vmId;
			}else{
				istringstream ss(string(begin+index2+1, s.end()-1));
				ss >> vmId;
			}
		}
		~RequestNode(){}
	public:
		string operation;
		string type;
		int vmId;
};

// 已部署的物理机
class DeployServer{
	public:
		DeployServer(const ServerNode &item)
			:type(item.type),
			totalSpace(item.nodeVec),
			remainSpace(item.nodeVec),
			occupySize(0), ratio(0){
				serverId = beginID;
				++beginID;
			}
		~DeployServer(){}
		void add(int vmId, const string &type, int nodeIndex);
		void del(int vmId, const string &type, int nodeIndex);
	private:
		void UpdateRatio();
		void UpdateOccupySize();
	public:
		static int beginID;	
		int serverId;						// 物理机编号
		string type;						// 物理机类型
		vector<struct node> totalSpace;		// 该物理机总空间
		vector<struct node> remainSpace;	// 该物理机当前剩余空间
		list<int> vmIdList;					// 部署在该物理机上的虚拟机链表
		int occupySize;
		float ratio;						// 该物理机的资源占用比
};
int DeployServer::beginID = 0;	

// 已部署的虚拟机
class DeployVM{
	public:
		DeployVM(int t_vmId, string t_type, int t_serverId, int t_nodeIndex)
			:vmId(t_vmId), type(t_type), serverId(t_serverId), nodeIndex(t_nodeIndex)
		{}
		~DeployVM(){}

	public:
		int vmId;		// 该虚拟机的唯一ID号
		string type;	// 该虚拟机的型号
		int serverId;	// 该虚拟机被部署的服务器ID
		int nodeIndex;	// 被部署到服务器的哪个NUMA节点（0->A, 1->B, 2->双节点部署）
};

// 服务器性价比排序，用于购买时参考
class ServerSortByCPR{
	public:
		ServerSortByCPR();
		~ServerSortByCPR(){}
		string FindBestServer(int needCpu, int needMemory);
		void PrintSortResult();
	private:
		vector<ServerNode*> blance;
		vector<ServerNode*> moreCpu;
		vector<ServerNode*> moreMemory;
};

// 调度器
class ServerScheduling{
	public:
		ServerScheduling():totalCost(0){}
		~ServerScheduling(){}
		void RequestHanlder(const RequestNode &request);
		void clear(){
			purchaseRecord.erase(purchaseRecord.begin(), purchaseRecord.end());
			migrationRecord.clear();
			deployRecord.clear();
			id = DeployServer::beginID;
		}
		void migration();
		void print();
		void CompuEleCost();
		void PrintTotalCost();
	private:
		void swap_id(int serverId1, int serverId2);
		void DeployResult(int serverId, int nodeIndex);
		void MigrationResult(int vmId, int serverId, int nodeIndex);
		int AddVM(const string &type, int vmId, int serverId, int nodeIndex);	// 执行添加虚拟机操作
		int DelVM(int vmId);											// 执行删除虚拟机操作
		bool FindInSingle(const int cpu, const int mem, int &serverId, int &nodeIndex);
		bool FindInSingle2(const int cpu, const int mem, int &serverId, int &nodeIndex);
		bool FindInMoreCpu(const int cpu, const int mem, int &serverId, int &nodeIndex);
		bool FindInMoreMemory(const int cpu, const int mem, int &serverId, int &nodeIndex);
		bool FindInBlance(const int cpu, const int mem, int &serverId, int &nodeIndex);

		bool FindAndAdd(const string &type, int vmId, const string &operation);

		void UpdateServer(int serverId);
		int Purchase(int cpu, int memory);
	private:
		ServerSortByCPR serverSell;		 //提供函数FindBestServer(int,int)
		unordered_map<int, DeployServer*> deployServerMap;		// 已购买的物理机数组
		unordered_map<int, DeployVM*> vmAddedMap;	// 被部署后的虚拟机映射

		//用于选择调度
		unordered_map<int, DeployServer*> moreCpuServer;
		unordered_map<int, DeployServer*> moreMemoryServer;
		unordered_map<int, DeployServer*> blanceServer;
		unordered_map<int ,DeployServer*> singleNode;

		//存储打印信息
		vector<pair<string, int>> purchaseRecord;

		vector<string> migrationRecord;	//用于记录迁移过程
		vector<DeployVM> migrationVM;	//备份迁移前位置

		vector<pair<int, string>> deployRecord;
		int id;

		//总成本
		int totalCost;
};


ostream & operator<<(ostream &, const ServerNode &);
ostream & operator<<(ostream &, const VirtualNode &);
ostream & operator<<(ostream &, const RequestNode &);
bool cmp(ServerNode *lhs, ServerNode *rhs);
bool cmp2(DeployServer *lhs, DeployServer *rhs);
bool cmp3(pair<int, string> &lhs, pair<int, string> &rhs);
void printServerInfo();
void printVirtualInfo();
void printRequestPerDay();

void readFromStream(istream &);					// 读入数据

unordered_map<string, ServerNode*> serverInfo;	// 服务器可购买的类型
unordered_map<string, VirtualNode*> virtualInfo;// 虚拟机类型
queue<list<RequestNode>*> requestPerDay;		// 请求序列

int main(int argc, char* argv[])
{
#ifdef LOCAL_DEBUG
	vector<string> file;
	file.push_back("/root/training-1.txt");
	file.push_back("/root/training-2.txt");
	ifstream ifs;
	/* ./exe 0 1.8 0.4 2.0 2.0 100 */
	if(argc == 7){
		int i = atoi(argv[1]);
		ifs.open(file[i]);
		if( !ifs.is_open() ){
			cout << "ifs open failed." << endl;
			exit(-1);
		}
		istringstream ss_yearOfN(argv[2]);
		istringstream ss_a(argv[3]);
		istringstream ss_cm(argv[4]);
		istringstream ss_mc(argv[5]);
		istringstream ss_ratio(argv[6]);
		istringstream ss_cpu_weight(argv[7]);
		istringstream ss_mem_weight(argv[8]);
		ss_yearOfN >> yearOfN;
		ss_a >> aOfCpu;
		ss_cm >> threshold_cpu_memory;
		ss_mc >> threshold_memory_cpu;
		ss_ratio >> max_ratio;
		ss_cpu_weight >> cpuWeight;
		ss_mem_weight >> memWeight;
	}else{
		ifs.open(file[0]);
		if( !ifs.is_open() ){
			cout << "ifs open failed." << endl;
			exit(-1);
		}
	}
	readFromStream(ifs);
#endif
#ifndef LOCAL_DEBUG
	readFromStream(cin);
#endif
	ServerScheduling scheduler;	//调度器

	list<RequestNode> *pReqList = nullptr;
	// 遍历每天的请求
	while( !requestPerDay.empty() ){
		pReqList = requestPerDay.front();
		requestPerDay.pop();

		scheduler.clear();
		scheduler.migration();
		for( const auto &request: *pReqList){
			scheduler.RequestHanlder(request);
		}
#ifndef LOCAL_DEBUG
		scheduler.print();
#endif
#ifdef LOCAL_DEBUG
		scheduler.CompuEleCost();
#endif
	}
#ifdef LOCAL_DEBUG
	scheduler.PrintTotalCost();
#endif

	return 0;
}


// 读数据
void readFromStream(istream &is){
	string temp;
	int N;	// N种类型物理机
	is >> N;
	getline(is, temp); //用于读走后面的换行符
	for(int i=0; i<N; ++i){
		string line;
		getline(is, line);
		ServerNode *p = new ServerNode(line);
		serverInfo.insert({p->type, p});
	}

	int M;	// M种类型虚拟机
	is >> M;
	getline(is, temp); //用于读走后面的换行符
	for(int i=0; i<M; ++i){
		string line;
		getline(is, line);
		VirtualNode *p = new VirtualNode(line);
		virtualInfo.insert({p->type, p});
	}

	int T;	// 连续T天的请求序列
	is >> T;
	getline(is, temp); //用于读走后面的换行符
	for(int i=0; i<T; ++i){
		int R;	// 当天有R条请求，用链表存储
		is >> R;
		getline(is, temp); //用于读走后面的换行符
		list<RequestNode> *p = new list<RequestNode>;
		for(int j=0; j<R; ++j){
			string line;
			getline(is, line);
			p->push_back(RequestNode(line));
		}
		requestPerDay.push(p);
	}
	return;
}

void ServerScheduling::swap_id(int serverId1, int serverId2){
	DeployServer *p1 = deployServerMap[serverId1];
	DeployServer *p2 = deployServerMap[serverId2];
	//修改DeployServer, DeployVM对象
	p1->serverId = serverId2;
	p2->serverId = serverId1;
	for(const auto &it : p1->vmIdList){
		vmAddedMap[it]->serverId = p1->serverId;
	}
	for(const auto &it : p2->vmIdList){
		vmAddedMap[it]->serverId = p2->serverId;
	}
	//修改deployServerMap
	deployServerMap[serverId1] = p2;
	deployServerMap[serverId2] = p1;
	//修改调度器moreCpuServer,moreMemoryServer,blanceServer,singleNode
	if( moreCpuServer.find(serverId1) != moreCpuServer.end() &&
			moreCpuServer.find(serverId2) != moreCpuServer.end() ){
		moreCpuServer[serverId1] = p2;
		moreCpuServer[serverId2] = p1;
	}else{
		if(moreCpuServer.find(serverId1) != moreCpuServer.end()){
			moreCpuServer.erase(serverId1);
			moreCpuServer.insert({serverId2, p1});
		}else if(moreCpuServer.find(serverId2) != moreCpuServer.end()){
			moreCpuServer.erase(serverId2);
			moreCpuServer.insert({serverId1, p2});
		}
	}
	//
	if( moreMemoryServer.find(serverId1) != moreMemoryServer.end() &&
			moreMemoryServer.find(serverId2) != moreMemoryServer.end() ){
		moreMemoryServer[serverId1] = p2;
		moreMemoryServer[serverId2] = p1;
	}else{
		if(moreMemoryServer.find(serverId1) != moreMemoryServer.end()){
			moreMemoryServer.erase(serverId1);
			moreMemoryServer.insert({serverId2, p1});
		}else if(moreMemoryServer.find(serverId2) != moreMemoryServer.end()){
			moreMemoryServer.erase(serverId2);
			moreMemoryServer.insert({serverId1, p2});
		}
	}
	//
	if( blanceServer.find(serverId1) != blanceServer.end() &&
			blanceServer.find(serverId2) != blanceServer.end() ){
		blanceServer[serverId1] = p2;
		blanceServer[serverId2] = p1;
	}else{
		if(blanceServer.find(serverId1) != blanceServer.end()){
			blanceServer.erase(serverId1);
			blanceServer.insert({serverId2, p1});
		}else if(blanceServer.find(serverId2) != blanceServer.end()){
			blanceServer.erase(serverId2);
			blanceServer.insert({serverId1, p2});
		}
	}
	//
	if( singleNode.find(serverId1) != singleNode.end() &&
			singleNode.find(serverId2) != singleNode.end() ){
		singleNode[serverId1] = p2;
		singleNode[serverId2] = p1;
	}else{
		if(singleNode.find(serverId1) != singleNode.end()){
			singleNode.erase(serverId1);
			singleNode.insert({serverId2, p1});
		}else if(singleNode.find(serverId2) != singleNode.end()){
			singleNode.erase(serverId2);
			singleNode.insert({serverId1, p2});
		}
	}
	//修改deployRecord
	for(auto &it : deployRecord){
		if(it.first==serverId1 || it.first==serverId2){
			it.first = (it.first==serverId1 ? serverId2 : serverId1);
		}
	}
}

void ServerScheduling::DeployResult(int serverId, int nodeIndex){
	string s;
	if(nodeIndex != 2){
		s = (0==nodeIndex ? "A" : "B");
	}
	deployRecord.push_back({serverId, s});
}
void ServerScheduling::MigrationResult(int vmId, int serverId, int nodeIndex){
	//验证是否在同一位置做迁移
	auto p = migrationVM.begin();
	for( ; p!=migrationVM.end(); ++p){
		if(p->vmId == vmId){
			break;
		}
	}
	if(p->vmId==vmId && p->serverId==serverId && p->nodeIndex==nodeIndex){
		return;
	}
	string s;
	s = string("(") + to_string(vmId) + string(", ") + to_string(serverId);
	if(nodeIndex != 2){
		string s_node = (0==nodeIndex ? "A" : "B");
		s = s + string(", ") + s_node;
	}
	s = s + string(")");
	migrationRecord.push_back(s);
}

void ServerScheduling::RequestHanlder(const RequestNode &request){
	if(request.operation == "del"){
		int serverId = DelVM(request.vmId);
		UpdateServer(serverId);
	}else{
		if( !FindAndAdd(request.type, request.vmId, "deploy") ){
			// 添加失败，购买新服务器
			VirtualNode *p = virtualInfo[request.type];
			int serverId = Purchase(p->cpuSize, p->memorySize);
			int nodeIndex = 0;
			if(p->twoNode == 1){	nodeIndex = 2; 	}
			AddVM(request.type, request.vmId, serverId, nodeIndex);
			DeployResult(serverId, nodeIndex);
			UpdateServer(serverId);
		}
	}	
}

/*判断是否需要迁移，选择需要迁移的服务器*/
void ServerScheduling::migration(){
	migrationRecord.clear();
	migrationVM.clear();	//确保迁移前该备份为空
	int numOfMigration = 5 * vmAddedMap.size() / 1000;
	float totalRatio = 0;
	int n = 0;
	for(const auto &it : deployServerMap){
		if(it.second->vmIdList.empty()){
			continue;
		}
		totalRatio += it.second->ratio;
		++n;
	}
	if(0 != n){
		totalRatio = totalRatio / n * 25;	// 25
	}
	//cout << "totalRatio: " << totalRatio << endl;
	
	//当资源使用率非常低，且5n/1000大于0时，进行迁移
	if( numOfMigration>0 && totalRatio<max_ratio ){
		vector<DeployServer*> servers;
		for(const auto &it : deployServerMap){
			if(it.second->vmIdList.empty()){
				continue;
			}
			servers.push_back(it.second);
		}
		sort(servers.begin(), servers.end(), cmp2);
		// 选择需要迁移的虚拟机
		for(const auto &p : servers){
			if(numOfMigration <= 0){
				break;
			}

			// 虚拟机链表copy一份，排序用
			vector<pair<int, string>> vms;
			for(auto i : p->vmIdList){
				vms.push_back({i, vmAddedMap[i]->type});
			}
			sort(vms.begin(), vms.end(), cmp3);
			for(const auto &it : vms){
				if(numOfMigration>0){
					DeployVM *vm_p = vmAddedMap[it.first];
					migrationVM.push_back({vm_p->vmId, vm_p->type, vm_p->serverId, vm_p->nodeIndex});
					--numOfMigration;
				}
			}
		}
		// 删除1台虚拟机，迁移1台虚拟机
		for(const auto &it : migrationVM){
			// 删除
			int serverId = DelVM(it.vmId);
			UpdateServer(serverId);
			// 迁移
			if( !FindAndAdd(it.type, it.vmId, "migration") ){
				AddVM(it.type, it.vmId, it.serverId, it.nodeIndex);
				UpdateServer(it.serverId);
			}
		}
	}
}

void ServerScheduling::print(){
	//调整新购买的服务器编号
	for(int i=id; i+1<DeployServer::beginID; ++i){
		if(i==id || deployServerMap[i]->type!=deployServerMap[i-1]->type){
			for(int j=i+1; j<DeployServer::beginID; ++j){
				if(deployServerMap[i]->type == deployServerMap[j]->type){
					int k = j;
					while(deployServerMap[k-1]->type != deployServerMap[k]->type){
						swap_id(k-1, k);
						--k;
					}
				}
			}
		}
	}
	//打印购买信息
	cout << "(purchase, " << purchaseRecord.size() << ")" << endl;
	for(const auto &it : purchaseRecord){
		cout << "(" << it.first << ", " << it.second << ")" << endl;
	}
	//打印迁移信息
	cout << "(migration, " << migrationRecord.size() << ")" << endl;
	for(const auto &it : migrationRecord){
		cout << it << endl;
	}
	//打印每个请求的处理结果
	for(const auto &it : deployRecord){
		cout << "(" << it.first;
		if( !it.second.empty() ){
			cout << ", " << it.second;
		}
		cout << ")" << endl;
	}
}

void ServerScheduling::CompuEleCost(){
	for(const auto &it : deployServerMap){
		if( !it.second->vmIdList.empty() ){
			totalCost += serverInfo[it.second->type]->costPerDay;
		}
	}
}
void ServerScheduling::PrintTotalCost(){
	for(const auto &it : deployServerMap){
		totalCost += serverInfo[it.second->type]->costBuy;
	}
	cout << totalCost << endl;
}

ServerSortByCPR::ServerSortByCPR(){
	for( const auto &it: serverInfo ){
		ServerNode *p = it.second;
		if( IsMoreCpu(p->nodeVec[0].cpuSize, p->nodeVec[0].memorySize) ){
			moreCpu.push_back(p);
		}else if( IsMoreMemory(p->nodeVec[0].cpuSize, p->nodeVec[0].memorySize) ){
			moreMemory.push_back(p);
		}else{
			blance.push_back(p);
		}
	}
	// sort by cost performance ratio
	sort(blance.begin(), blance.end(), cmp);
	sort(moreCpu.begin(), moreCpu.end(), cmp);
	sort(moreMemory.begin(), moreMemory.end(), cmp);
}
string ServerSortByCPR::FindBestServer(int needCpu, int needMemory){
	if( IsMoreCpu(needCpu, needMemory) ){
		for( const auto it: moreCpu ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
		for( const auto it: blance ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
		for( const auto it: moreMemory ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
	}else if( IsMoreMemory(needCpu, needMemory) ){
		for( const auto it: moreMemory ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
		for( const auto it: blance ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
		for( const auto it: moreCpu ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
	}else{
		for( const auto it: blance ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
		for( const auto it: moreMemory ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
		for( const auto it: moreCpu ){
			if(it->nodeVec[0].cpuSize >= needCpu && it->nodeVec[0].memorySize >= needMemory){
				return it->type;
			}
		}
	}
	return string();
}

void ServerSortByCPR::PrintSortResult(){
	cout << "moreCpu:" << endl;
	for(const auto &it : moreCpu){
		cout << *it << endl;
	}
	cout << "moreMemory:" << endl;
	for(const auto &it : moreMemory){
		cout << *it << endl;
	}
	cout << "blance:" << endl;
	for(const auto &it : blance){
		cout << *it << endl;
	}
}

void DeployServer::add(int vmId, const string &type, int nodeIndex){
	if( 0==nodeIndex || 1==nodeIndex ){
		remainSpace[nodeIndex].cpuSize -= virtualInfo[type]->cpuSize;
		remainSpace[nodeIndex].memorySize -= virtualInfo[type]->memorySize;
	}else{
		remainSpace[0].cpuSize -= virtualInfo[type]->cpuSize/2;
		remainSpace[0].memorySize -= virtualInfo[type]->memorySize/2;
		remainSpace[1].cpuSize -= virtualInfo[type]->cpuSize/2;
		remainSpace[1].memorySize -= virtualInfo[type]->memorySize/2;
	}
	if( remainSpace[0].cpuSize < 0 ||
			remainSpace[0].memorySize < 0 ||
			remainSpace[1].cpuSize < 0 ||
			remainSpace[1].memorySize < 0 ){
		cout << "error remainSpace < 0" << endl;
		exit(1);
	}
	vmIdList.push_back(vmId);
	UpdateRatio();
	UpdateOccupySize();
}
void DeployServer::del(int vmId, const string &type, int nodeIndex){
	if( 0==nodeIndex || 1==nodeIndex ){
		remainSpace[nodeIndex].cpuSize += virtualInfo[type]->cpuSize;
		remainSpace[nodeIndex].memorySize += virtualInfo[type]->memorySize;
	}else{
		remainSpace[0].cpuSize += virtualInfo[type]->cpuSize/2;
		remainSpace[0].memorySize += virtualInfo[type]->memorySize/2;
		remainSpace[1].cpuSize += virtualInfo[type]->cpuSize/2;
		remainSpace[1].memorySize += virtualInfo[type]->memorySize/2;
	}
	auto it = vmIdList.begin();
	for( ; it!=vmIdList.end(); ++it){
		if(*it == vmId){
			break;
		}
	}
	vmIdList.erase(it);
	UpdateRatio();
	UpdateOccupySize();
}
void DeployServer::UpdateRatio(){
	ratio  = (totalSpace[0].cpuSize-remainSpace[0].cpuSize)*1.0/totalSpace[0].cpuSize;
	ratio += (totalSpace[1].cpuSize-remainSpace[1].cpuSize)*1.0/totalSpace[1].cpuSize;
	ratio += (totalSpace[0].memorySize-remainSpace[0].memorySize)*1.0/totalSpace[0].memorySize;
	ratio += (totalSpace[1].memorySize-remainSpace[1].memorySize)*1.0/totalSpace[1].memorySize;
}
void DeployServer::UpdateOccupySize(){
	occupySize  = (totalSpace[0].cpuSize-remainSpace[0].cpuSize);
	occupySize += (totalSpace[1].cpuSize-remainSpace[1].cpuSize);
	occupySize += (totalSpace[0].memorySize-remainSpace[0].memorySize);
	occupySize += (totalSpace[1].memorySize-remainSpace[1].memorySize);
}

int ServerScheduling::AddVM(const string &type, int vmId, int serverId, int nodeIndex){
	// 新增vmId,type类型的虚拟机到vmAddedMap
	DeployVM *p = nullptr;
	if( vmAddedMap.find(vmId) == vmAddedMap.end() ){
		p = new DeployVM(vmId, type, serverId, nodeIndex);
		vmAddedMap.insert({vmId, p});
	}else{
		cout << "vmId重复添加(vmId:" << vmId << " type:" << type << endl;
		exit(1);
	}
	// 将vm添加到对应服务器
	deployServerMap[serverId]->add(vmId, type, nodeIndex);
	return serverId;
}

int ServerScheduling::DelVM(int vmId){
	int serverId;
	DeployVM *p = vmAddedMap[vmId];
	if( nullptr == p ){
		cout << "delVM error" << endl;
		exit(1);
	}
	// 将对应服务器的vm删除
	serverId = p->serverId;
	deployServerMap[p->serverId]->del(vmId, p->type, p->nodeIndex);
	// 将vmId对应的虚拟机删除
	delete p;
	vmAddedMap.erase(vmId);
	return serverId;
}

bool ServerScheduling::FindInSingle(const int cpu, const int mem, int &serverId, int &nodeIndex){
	int bestId, bestNodeIndex;
	double value = -1;

	for( const auto &it : singleNode ){
		if( cpu <= it.second->remainSpace[0].cpuSize &&
				mem <= it.second->remainSpace[0].memorySize){
			double t_value = 1.0*cpu/it.second->remainSpace[0].cpuSize
				+ 1.0*mem/it.second->remainSpace[0].memorySize;
			if(value==-1 || value<t_value){
				value = t_value;
				bestId = it.second->serverId;
				bestNodeIndex = 0;
			}
		}
		if( cpu <= it.second->remainSpace[1].cpuSize &&
				mem <= it.second->remainSpace[1].memorySize){
			double t_value = 1.0*cpu/it.second->remainSpace[1].cpuSize
				+ 1.0*mem/it.second->remainSpace[1].memorySize;
			if(value==-1 || value<t_value){
				value = t_value;
				bestId = it.second->serverId;
				bestNodeIndex = 1;
			}
		}
	}
	if(-1 != value){
		serverId = bestId;
		nodeIndex = bestNodeIndex;
		return true;
	}
	return false;
}

bool ServerScheduling::FindInSingle2(const int cpu, const int mem, int &serverId, int &nodeIndex){
	int bestId, bestNodeIndex;
	double value = -1;

	for( const auto &it : singleNode ){
		if( cpu <= it.second->remainSpace[0].cpuSize &&
				mem <= it.second->remainSpace[0].memorySize &&
				cpu <= it.second->remainSpace[1].cpuSize &&
				mem <= it.second->remainSpace[1].memorySize ){
			double t_value = 1.0*cpu/it.second->remainSpace[0].cpuSize
				+ 1.0*mem/it.second->remainSpace[0].memorySize
				+ 1.0*cpu/it.second->remainSpace[1].cpuSize
				+ 1.0*mem/it.second->remainSpace[1].memorySize;
			if(value==-1 || value<t_value){
				value = t_value;
				bestId = it.second->serverId;
				bestNodeIndex = 0;
			}
		}
	}
	if(-1 != value){
		serverId = bestId;
		nodeIndex = bestNodeIndex;
		return true;
	}
	return false;
}

bool ServerScheduling::FindInMoreCpu(const int cpu, const int mem, int &serverId, int &nodeIndex){
	int bestId, bestNodeIndex;
	double value = -1;

	for( const auto &it : moreCpuServer ){
		if( cpu <= it.second->remainSpace[0].cpuSize &&
				mem <= it.second->remainSpace[0].memorySize){
			double t_value = 1.0*cpu/it.second->remainSpace[0].cpuSize
				+ 1.0*mem/it.second->remainSpace[0].memorySize;
			if(value==-1 || value<t_value){
				value = t_value;
				bestId = it.second->serverId;
				bestNodeIndex = 0;
			}
		}
	}
	if(-1 != value){
		serverId = bestId;
		nodeIndex = bestNodeIndex;
		return true;
	}
	return false;
}

bool ServerScheduling::FindInMoreMemory(const int cpu, const int mem, int &serverId, int &nodeIndex){
	int bestId, bestNodeIndex;
	double value = -1;

	for( const auto &it : moreMemoryServer ){
		if( cpu <= it.second->remainSpace[0].cpuSize &&
				mem <= it.second->remainSpace[0].memorySize){
			double t_value = 1.0*cpu/it.second->remainSpace[0].cpuSize
				+ 1.0*mem/it.second->remainSpace[0].memorySize;
			if(value==-1 || value<t_value){
				value = t_value;
				bestId = it.second->serverId;
				bestNodeIndex = 0;
			}
		}
	}
	if(-1 != value){
		serverId = bestId;
		nodeIndex = bestNodeIndex;
		return true;
	}
	return false;
}

bool ServerScheduling::FindInBlance(const int cpu, const int mem, int &serverId, int &nodeIndex){
	int bestId, bestNodeIndex;
	double value = -1;

	for( const auto &it : blanceServer ){
		if( cpu <= it.second->remainSpace[0].cpuSize &&
				mem <= it.second->remainSpace[0].memorySize){
			double t_value = 1.0*cpu/it.second->remainSpace[0].cpuSize
				+ 1.0*mem/it.second->remainSpace[0].memorySize;
			if(value==-1 || value<t_value){
				value = t_value;
				bestId = it.second->serverId;
				bestNodeIndex = 0;
			}
		}
	}
	if(-1 != value){
		serverId = bestId;
		nodeIndex = bestNodeIndex;
		return true;
	}
	return false;
}

bool ServerScheduling::FindAndAdd(const string &type, int vmId, const string &operation){
	VirtualNode *p = virtualInfo[type];
	int needCpu, needMemory;
	int serverId = -1, nodeIndex = -1;

	if(0 == p->twoNode){
		needCpu = p->cpuSize;
		needMemory = p->memorySize;
		// 先单节点找
		if( !FindInSingle(needCpu, needMemory, serverId, nodeIndex) ){
			if( IsMoreCpu(needCpu, needMemory) ){
				if( !FindInMoreCpu(needCpu, needMemory, serverId, nodeIndex) ){
					if( !FindInBlance(needCpu, needMemory, serverId, nodeIndex) ){
					//	FindInMoreMemory(needCpu, needMemory, serverId, nodeIndex);
					}
				}
			}else if( IsMoreMemory(needCpu, needMemory) ){
				if( !FindInMoreMemory(needCpu, needMemory, serverId, nodeIndex) ){
					if( !FindInBlance(needCpu, needMemory, serverId, nodeIndex) ){
					//	FindInMoreCpu(needCpu, needMemory, serverId, nodeIndex);
					}
				}
			}else{
				if( !FindInBlance(needCpu, needMemory, serverId, nodeIndex) ){
					if( !FindInMoreMemory(needCpu, needMemory, serverId, nodeIndex) ){
						FindInMoreCpu(needCpu, needMemory, serverId, nodeIndex);
					}
				}
			}
		}
		if(-1==serverId && -1==nodeIndex)	return false;
		AddVM(type, vmId, serverId, nodeIndex);
		//记录
		if(operation == string("deploy")){
			DeployResult(serverId, nodeIndex);
		}else{
			MigrationResult(vmId, serverId, nodeIndex);
		}
		UpdateServer(serverId);
		return true;
	}
	if(1 == p->twoNode){
		needCpu = p->cpuSize/2;
		needMemory = p->memorySize/2;

		if( IsMoreCpu(needCpu, needMemory) ){
			if( !FindInMoreCpu(needCpu, needMemory, serverId, nodeIndex) ){
				if( !FindInBlance(needCpu, needMemory, serverId, nodeIndex) ){
					//if( !FindInMoreMemory(needCpu, needMemory, serverId, nodeIndex) ){
					//	FindInSingle2(needCpu, needMemory, serverId, nodeIndex);
					//}
				}
			}
		}else if( IsMoreMemory(needCpu, needMemory) ){
			if( !FindInMoreMemory(needCpu, needMemory, serverId, nodeIndex) ){
				if( !FindInBlance(needCpu, needMemory, serverId, nodeIndex) ){
					//if( !FindInMoreCpu(needCpu, needMemory, serverId, nodeIndex) ){
					//	FindInSingle2(needCpu, needMemory, serverId, nodeIndex);
					//}
				}
			}
		}else{
			if( !FindInBlance(needCpu, needMemory, serverId, nodeIndex) ){
				if( !FindInMoreMemory(needCpu, needMemory, serverId, nodeIndex) ){
					if( !FindInMoreCpu(needCpu, needMemory, serverId, nodeIndex) ){
						FindInSingle2(needCpu, needMemory, serverId, nodeIndex);
					}
				}
			}
		}
		if(-1==serverId && -1==nodeIndex)	return false;
		nodeIndex = 2;
		AddVM(type, vmId, serverId, nodeIndex);
		//记录
		if(operation == string("deploy")){
			DeployResult(serverId, nodeIndex);
		}else{
			MigrationResult(vmId, serverId, nodeIndex);
		}
		UpdateServer(serverId);
		return true;
	}
	cout << "FindAndADD error" << endl;
	exit(1);
}

void ServerScheduling::UpdateServer(int serverId){
	DeployServer *p = deployServerMap[serverId];
	if( p->remainSpace[0].cpuSize != p->remainSpace[1].cpuSize ||
			p->remainSpace[0].memorySize != p->remainSpace[1].memorySize){
		moreCpuServer.erase(serverId);
		moreMemoryServer.erase(serverId);
		blanceServer.erase(serverId);
		singleNode.insert({serverId, p});
	}else if( IsMoreCpu(p->remainSpace[0].cpuSize, p->remainSpace[0].memorySize) ){
		moreMemoryServer.erase(serverId);
		blanceServer.erase(serverId);
		singleNode.erase(serverId);
		moreCpuServer.insert({serverId, p});
	}else if( IsMoreMemory(p->remainSpace[0].cpuSize, p->remainSpace[0].memorySize) ){
		blanceServer.erase(serverId);
		singleNode.erase(serverId);
		moreCpuServer.erase(serverId);
		moreMemoryServer.insert({serverId, p});
	}else{
		singleNode.erase(serverId);
		moreCpuServer.erase(serverId);
		moreMemoryServer.erase(serverId);
		blanceServer.insert({serverId, p});
	}
}

int ServerScheduling::Purchase(int cpu, int memory){
	string serverType = serverSell.FindBestServer(cpu, memory);
	//cout << "purchase - (" << serverType << ", 1)" << endl;
	auto begin = purchaseRecord.begin();
	for( ; begin!=purchaseRecord.end(); ++begin){
		if(begin->first == serverType)
			break;
	}
	if(begin != purchaseRecord.end()){
		begin->second += 1;
	}else{
		purchaseRecord.push_back({serverType, 1});
	}
	DeployServer *p = new DeployServer(*serverInfo[serverType]);
	/*
	   cout << *serverInfo[serverType] << endl;
	   cout << "serverID: " << p->serverId << endl;
	   cout << "0cpu:" << p->remainSpace[0].cpuSize << " 0memory:" << p->remainSpace[0].memorySize << endl;
	 */
	deployServerMap.insert({p->serverId, p});
	if( IsMoreCpu(p->remainSpace[0].cpuSize, p->remainSpace[0].memorySize) ){
		moreCpuServer.insert({p->serverId, p});
	}else if( IsMoreMemory(p->remainSpace[0].cpuSize, p->remainSpace[0].memorySize) ){
		moreMemoryServer.insert({p->serverId, p});
	}else{
		blanceServer.insert({p->serverId, p});
	}
	return p->serverId;
}

ostream & operator<<(ostream &os, const ServerNode &item){
	double cost = item.costBuy + item.costPerDay * 365 * yearOfN;
	cost *= 1.0/item.nodeVec[0].memorySize + 1.0/item.nodeVec[0].cpuSize;
	os << item.type << " "
		<< item.nodeVec[0].cpuSize << " "
		<< item.nodeVec[0].memorySize << " "
		<< item.nodeVec[1].cpuSize << " "
		<< item.nodeVec[1].memorySize << " "
		<< item.costBuy << " "
		<< item.costPerDay << " "
		<< cost;
	return os;
}
ostream & operator<<(ostream &os, const VirtualNode &item){
	os << item.type << " "
		<< item.cpuSize << " "
		<< item.memorySize << " "
		<< item.twoNode;
	return os;
}
ostream & operator<<(ostream &os, const RequestNode &item){
	os << "operation:" << item.operation 
		<< " type:" << item.type
		<< " vmId:" << item.vmId;
	return os;
}
bool cmp(ServerNode *lhs, ServerNode *rhs){
	double lhs_cost = lhs->costBuy + lhs->costPerDay * 365 * yearOfN;
	double rhs_cost = rhs->costBuy + rhs->costPerDay * 365 * yearOfN;
	lhs_cost *= 1.0/lhs->nodeVec[0].memorySize*aOfCpu + 1.0/lhs->nodeVec[0].cpuSize*(1-aOfCpu);
	rhs_cost *= 1.0/rhs->nodeVec[0].memorySize*aOfCpu + 1.0/rhs->nodeVec[0].cpuSize*(1-aOfCpu);
	return (lhs_cost < rhs_cost);
}
bool cmp2(DeployServer *lhs, DeployServer *rhs){
	return (lhs->occupySize < rhs->occupySize);
}
bool cmp3(pair<int, string> &lhs, pair<int, string> &rhs){
	VirtualNode *p_lhs = virtualInfo[lhs.second];
	VirtualNode *p_rhs = virtualInfo[rhs.second];
	double l_value = cpuWeight * p_lhs->cpuSize + memWeight * p_lhs->memorySize;
	double r_value = cpuWeight * p_rhs->cpuSize + memWeight * p_rhs->memorySize;
	return (l_value > r_value);
}
void printServerInfo(){
	for( auto &it: serverInfo ){
		cout << "物理机：" << *(it.second) << endl;
	}
}
void printVirtualInfo(){
	for( auto &it: virtualInfo ){
		cout << "虚拟机：" << *(it.second) << endl;
	}
}
void printRequestPerDay(){
	list<RequestNode> *p;
	int n = 1;
	while( !requestPerDay.empty() ){
		p = requestPerDay.front();
		requestPerDay.pop();
		cout << "第 " << n << " 天" << endl;
		for( auto &it: *p ){
			cout << it << endl;
		}
		cout << endl;
		++n;
		break;
	}
}
