#include "MapMatching.h"
#include "ReadInTrajs.h"

//运行所需的全局变量
vector<string> outputFileNames;
list<Traj*> trajList;
string rootFilePath = "D:\\Document\\Subjects\\Computer\\Develop\\Data\\GISCUP2012_Data\\";
Map routeNetwork = Map(rootFilePath, 1000);
//保存计算过的两点间最短距离，键pair对表示起点和终点，值pair表示两点间最短距离和对应的deltaT
//保存的deltaT的原因是：如果deltaT过小，则返回的最短距离可能为INF；而当再遇到相同起点和终点、而deltaT变大时，最短距离可能就不是INF了
//类似的，当已保存的最短距离不是INF，而遇到某个更小的deltaT时，最短距离可能就是INF了
std::map<pair<int, int>, pair<double, double>> shortestDistPair = std::map<pair<int, int>, pair<double, double>>();
//ofstream logOutput;


//地图匹配所用数据结构
struct Score//代表某个轨迹点对应的一个候选路段
{
	Edge* edge;//候选路段的指针
	long double score;//候选路段所具有的整体概率
	int preColumnIndex;//候选路段的前序路段的列索引
	double distLeft;//轨迹点的投影点到候选路段起点的距离
	Score(Edge* edge, long double score, int pre, double distLeft){
		this->edge = edge;
		this->score = score;
		this->preColumnIndex = pre;
		this->distLeft = distLeft;
	}
};

//放射概率计算函数
//放射概率：使用轨迹点到候选路段的距离在高斯分布上的概率
//参数t：为Wang Yin算法而设，表示前后轨迹点间的时间差
//参数dist：轨迹点到候选路段的距离
double EmissionProb(double t, double dist){
	return t*sqrt(dist);
	//return t*sqrt(dist)*COEFFICIENT_FOR_EMISSIONPROB;
}

//辅助函数：给定行索引，计算scoreMatrix中该行中整体概率最大的候选路段的索引
int GetStartColumnIndex(vector<Score> &row){
	int resultIndex = -1;
	long double currentMaxProb = 1e10;
	for (size_t i = 0; i < row.size(); i++){
		if (currentMaxProb > row.at(i).score){
			currentMaxProb = row.at(i).score;
			resultIndex = i;
		}
	}
	return resultIndex;
}

//若相邻匹配路段不连接，则在两个路段间插入其最短路径通过的路段
list<Edge*> linkMatchedResult(list<Edge*> &mapMatchingResult){
	list<Edge*> result = list<Edge*>();
	Edge* lastEdge = NULL;
	for (list<Edge*>::iterator iter = mapMatchingResult.begin(); iter != mapMatchingResult.end(); iter++)
	{
		if (lastEdge == NULL){
			lastEdge = *iter;
			result.push_back(*iter);
			continue;
		}
		else{
			if ((*iter) == NULL){
				continue;
			}
			else{
				if (lastEdge != *iter&&lastEdge->endNodeId != (*iter)->startNodeId){
					list<Edge*> shortestPath = list<Edge*>();
					routeNetwork.shortestPathLength(lastEdge->endNodeId, (*iter)->startNodeId, shortestPath);
					result.insert(result.end(), shortestPath.begin(), shortestPath.end());
					result.push_back(*iter);
					lastEdge = *iter;
				}
			}
		}
	}
	return result;
}

list<Edge*> MapMatching(list<GeoPoint*> &trajectory){
	list<Edge*> mapMatchingResult;//全局匹配路径
	vector<vector<Score>> scoreMatrix = vector<vector<Score>>();//所有轨迹点的概率矩阵
	//需要在每次循环结束前更新的变量
	GeoPoint* formerTrajPoint = NULL;//上一个轨迹点，计算路网距离时需要
	bool cutFlag = true;//没有前一轨迹点或前一轨迹点没有达标的候选路段
	int currentTrajPointIndex = 0;//当前轨迹点的索引
	for (list<GeoPoint*>::iterator trajectoryIterator = trajectory.begin(); trajectoryIterator != trajectory.end(); trajectoryIterator++)//遍历每个轨迹点
	{
		double deltaT = 1;//当前序轨迹点存在时，delaT表示前后两轨迹点间的时间差
		if (formerTrajPoint != NULL){ deltaT = (*trajectoryIterator)->time - formerTrajPoint->time; }
		long double currentMaxProb = 1e10;//当前最大整体概率，初始值为1e10
		vector<Score> scores = vector<Score>();//当前轨迹点的Score集合
		vector<Edge*> canadidateEdges;//候选路段集合
		routeNetwork.getNearEdges((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, RANGEOFCANADIDATEEDGES, canadidateEdges);//获得所有在指定范围内的候选路段集合
		long double *emissionProbs = new long double[canadidateEdges.size()];//保存这些候选路段的放射概率
		int currentCanadidateEdgeIndex = 0;//当前候选路段的索引
		for each (Edge* canadidateEdge in canadidateEdges)
		{
			int preColumnIndex = -1;//保存当前候选路段的前序路段的列索引
			double currentDistLeft = 0;//当前轨迹点在候选路段上的投影点距路段起点的距离
			double DistBetweenTrajPointAndEdge = routeNetwork.distMFromTransplantFromSRC((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, canadidateEdge, currentDistLeft);
			//计算这些候选路段的放射概率
			emissionProbs[currentCanadidateEdgeIndex] = deltaT*sqrt(DistBetweenTrajPointAndEdge);
			if (!cutFlag){
				//如果当前采样点不是轨迹第一个点或匹配中断后的第一个点，则计算来自前序轨迹点匹配路段的转移概率
				long double currentMaxProbTmp = 1e10;//当前最大转移概率，初始值为1e10
				int formerCanadidateEdgeIndex = 0;
				for each(Score formerCanadidateEdge in scoreMatrix.back()){
					double formerDistLeft = formerCanadidateEdge.distLeft;//前一个轨迹点在候选路段上的投影点距路段起点的距离
					double formerDistToEnd = formerCanadidateEdge.edge->lengthM - formerDistLeft;//前一个轨迹点在候选路段上的投影点距路段终点的距离
					double routeNetworkDistBetweenTwoEdges;//两路段起点间的距离
					double routeNetworkDistBetweenTwoTrajPoints;//两轨迹点对应的投影点间的路网距离					
					if (canadidateEdge == formerCanadidateEdge.edge){//如果前一匹配路段和当前候选路段是同一路段，则两者计算距离起点的差即为路网距离
						routeNetworkDistBetweenTwoTrajPoints = fabs(currentDistLeft - formerCanadidateEdge.distLeft);
					}
					else
					{
						pair<int, int> odPair = make_pair(formerCanadidateEdge.edge->endNodeId, canadidateEdge->startNodeId);
						//给定起点和终点最短路已经计算过，且不是INF
						if (shortestDistPair.find(odPair) != shortestDistPair.end() && shortestDistPair[odPair].first < INF){
							//如果当前deltaT下的移动距离上限比最短距离要大，调用最短路函数得到的也是保存的距离值；反之得到的就是INF
							shortestDistPair[odPair].first <= MAXSPEED*deltaT ? routeNetworkDistBetweenTwoEdges = shortestDistPair[odPair].first : routeNetworkDistBetweenTwoEdges = INF;
						}
						else{
							if (shortestDistPair.find(odPair) != shortestDistPair.end() && deltaT <= shortestDistPair[odPair].second){//保存的给定起点和终点最短路结果是INF，且当前deltaT比上次计算最短路时的移动时间要小，说明当前deltaT下得到的最短路距离仍是INF
								routeNetworkDistBetweenTwoEdges = INF;
							}
							else{
								//或者未保存给定起点和终点的最短路结果；或者当前deltaT比保存的deltaT要大，可能得到真正的最短路结果；总之就是要调用函数计算最短路
								list<Edge*> shortestPath = list<Edge*>();
								routeNetworkDistBetweenTwoEdges = routeNetwork.shortestPathLength(formerCanadidateEdge.edge->endNodeId, canadidateEdge->startNodeId, shortestPath, currentDistLeft, formerDistToEnd, deltaT);
								shortestDistPair[odPair] = make_pair(routeNetworkDistBetweenTwoEdges, deltaT);
							}
						}
						routeNetworkDistBetweenTwoTrajPoints = routeNetworkDistBetweenTwoEdges + currentDistLeft + formerDistToEnd;
					}
					//double distBetweenTwoTrajPoints = GeoPoint::distM((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, formerTrajPoint->lat, formerTrajPoint->lon);//两轨迹点间的直接距离
					long double transactionProb = (long double)routeNetworkDistBetweenTwoTrajPoints*COEFFICIENT_FOR_TRANSATIONPROB;//转移概率
					long double tmpTotalProbForTransaction = formerCanadidateEdge.score + transactionProb;
					if (currentMaxProbTmp > tmpTotalProbForTransaction){//保留当前转移概率和已知最小转移概率中较小者（由于取log的缘故）
						currentMaxProbTmp = tmpTotalProbForTransaction;
						preColumnIndex = formerCanadidateEdgeIndex;
					}
					formerCanadidateEdgeIndex++;
				}
				//此时，emissionProbs保存的是候选路段的放射概率，加上转移概率则变为候选路段的整体概率（由于取log的缘故）
				emissionProbs[currentCanadidateEdgeIndex] += currentMaxProbTmp;
			}
			/*若需要提升代码运行速度，则只把整体概率大于MINPROB的候选路段放入当前轨迹点的Score集合中；否则把所有候选路段放入Score集合中*/
			scores.push_back(Score(canadidateEdge, emissionProbs[currentCanadidateEdgeIndex], preColumnIndex, currentDistLeft));
			//得到当前最小整体概率（由于取log的缘故），以便归一化
			if (currentMaxProb > emissionProbs[currentCanadidateEdgeIndex]){ currentMaxProb = emissionProbs[currentCanadidateEdgeIndex]; }
			currentCanadidateEdgeIndex++;
		}
		delete[]emissionProbs;
		formerTrajPoint = *trajectoryIterator;
		currentTrajPointIndex++;
		//for (int i = 0; i < scores.size(); i++)	{ scores[i].score /= currentMaxProb; }//归一化		
		scoreMatrix.push_back(scores);//把该轨迹点的Scores数组放入scoreMatrix中
		if (scores.size() == 0){//若scores数组为空，则说明没有一个达标的候选路段，cutFlag设为true，后续轨迹作为新轨迹进行匹配
			cutFlag = true;
			formerTrajPoint = NULL;
		}
		else
		{
			cutFlag = false;
		}
	}

	//得到全局匹配路径
	int startColumnIndex = GetStartColumnIndex(scoreMatrix.back());//得到最后一个轨迹点的在scoreMatrix对应行中得分最高的列索引，即全局匹配路径的终点
	for (int i = scoreMatrix.size() - 1; i >= 0; i--){
		if (startColumnIndex != -1){
			mapMatchingResult.push_front(scoreMatrix[i][startColumnIndex].edge);
			startColumnIndex = scoreMatrix[i][startColumnIndex].preColumnIndex;
		}
		else
		{
			mapMatchingResult.push_front(NULL);
			if (i > 0){
				startColumnIndex = GetStartColumnIndex(scoreMatrix[i - 1]);
			}
		}
	}
	//调试代码：输出最终的概率矩阵：如果有某个轨迹点的所有候选路段的整体概率均为均为无穷小/无穷大，那可能就不正常，需要进一步检查该行概率的得到过程
	//for (int i = 0; i < scoreMatrix.size(); i++){
	//	logOutput << scoreMatrix.at(i).size() << "\t";
	//	for (int j = 0; j < scoreMatrix.at(i).size(); j++){
	//		logOutput << "[" << scoreMatrix.at(i).at(j).edge->id << "][" << scoreMatrix.at(i).at(j).preColumnIndex << "][" << scoreMatrix.at(i).at(j).score << "]\t";
	//	}
	//	logOutput << endl;
	//}
	//调试结束

	return mapMatchingResult;
	//return linkMatchedResult(mapMatchingResult);
}

void main(){
	//logOutput = ofstream("debug.txt");
	//logOutput.setf(ios::showpoint);
	//logOutput.precision(8);
	scanTrajFolder(rootFilePath, trajList, outputFileNames);
	int trajIndex = 0;
	cout << "开始地图匹配！" << endl;
	for (list<Traj*>::iterator trajIter = trajList.begin(); trajIter != trajList.end(); trajIter++){
		list<Edge*> resultList = MapMatching(*(*trajIter));
		ofstream MatchedEdgeOutput(rootFilePath + "output\\" + outputFileNames[trajIndex]);
		Traj::iterator trajPointIter = (*trajIter)->begin();
		for (list<Edge*>::iterator edgeIter = resultList.begin(); edgeIter != resultList.end(); edgeIter++, trajPointIter++){
			if (*edgeIter != NULL){
				int currentIndex = (*edgeIter)->id;
				//MatchedEdgeOutput << (*trajPointIter)->time << "," << currentIndex << ",1.0" << endl;
				MatchedEdgeOutput << "0," << currentIndex << ",1.0" << endl;
			}
			else{
				//MatchedEdgeOutput << (*trajPointIter)->time << "," << -1 << ",1.0" << endl;
				MatchedEdgeOutput << "0," << -1 << ",1.0" << endl;
			}
		}
		MatchedEdgeOutput.close();
		cout << "第" << trajIndex << "条轨迹匹配完毕！" << endl;
		trajIndex++;
	}
	//logOutput.close();
}