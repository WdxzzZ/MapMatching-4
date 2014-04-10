#include <sstream>
#include "MapMatching.h"
#include "baiduMap.h"
#include "baiduTrajReader.h"
#include "MapDrawer.h"

//���������ȫ�ֱ���
vector<string> outputFileNames;
list<Traj*> trajList;
string rootFilePath = "D:\\Document\\MDM Lab\\Data\\LBS_DATASET\\";
BaiduMap routeNetwork = BaiduMap(rootFilePath, 1000);
//�����������������̾��룬��pair�Ա�ʾ�����յ㣬ֵpair��ʾ�������̾���Ͷ�Ӧ��deltaT
//�����deltaT��ԭ���ǣ����deltaT��С���򷵻ص���̾������ΪINF��������������ͬ�����յ㡢��deltaT���ʱ����̾�����ܾͲ���INF��
//���Ƶģ����ѱ������̾��벻��INF��������ĳ����С��deltaTʱ����̾�����ܾ���INF��
std::map<pair<int, int>, pair<double, double>> shortestDistPair = std::map<pair<int, int>, pair<double, double>>();
//ofstream logOutput;


/*Ϊ�ٶ�LBS�������޸�*/
MapDrawer md;
double minLat = 31.853;
double maxLat = 31.893;
double minLon = 117.250;
double maxLon = 117.336;
/*Ϊ�ٶ�LBS�������޸�*/


//�ڲ�ͬ�����ʣ�1~30s/�㣩�¸���beta��ֵ������ó�= =
const double BETA_ARR[31] = {
	0,
	0.490376731, 0.82918373, 1.24364564, 1.67079581, 2.00719298,
	2.42513007, 2.81248831, 3.15745473, 3.52645392, 4.09511775,
	4.67319795, 5.41088180, 6.47666590, 6.29010734, 7.80752112,
	8.09074504, 8.08550528, 9.09405065, 11.09090603, 11.87752824,
	12.55107715, 15.82820829, 17.69496773, 18.07655652, 19.63438911,
	25.40832185, 23.76001877, 28.43289797, 32.21683062, 34.56991141
};

string ToString(int i){
	stringstream ss;
	ss << i;
	return ss.str();
}

//��ͼƥ���������ݽṹ
struct Score//����ĳ���켣���Ӧ��һ����ѡ·��
{
	Edge* edge;//��ѡ·�ε�ָ��
	/*Ϊ�ٶ�LBS�������޸�*/
	double distBetweenTrajPointAndEdge;//�켣�㵽��ѡ·�ε�ͶӰ����
	double emissionProb;//��¼�켣�㵽��ѡ·�εķ������
	double distBetweenTwoTrajPoints;//�켣���ǰ��켣����������
	double routeNetworkDistBetweenTwoTrajPoints;//��¼�ú�ѡ·�κ�ǰ��·�ε�·������
	long double transactionProb;//��ѡ·�������е�ת�Ƹ���
	/*Ϊ�ٶ�LBS�������޸�*/
	long double score;//��ѡ·�������е��������
	int preColumnIndex;//��ѡ·�ε�ǰ��·�ε�������
	double distLeft;//�켣���ͶӰ�㵽��ѡ·�����ľ���
	Score(Edge* edge, long double score, int pre, double distLeft, double distBetweenTrajPointAndEdge = 0, double emissionProb = 0, double distBetweenTwoTrajPoints = 0, double routeNetworkDistBetweenTwoTrajPoints = 0, double transactionProb = 0){
		this->edge = edge;
		this->score = score;
		this->preColumnIndex = pre;
		this->distLeft = distLeft;
		/*Ϊ�ٶ�LBS�������޸�*/
		this->distBetweenTrajPointAndEdge = distBetweenTrajPointAndEdge;
		this->emissionProb = emissionProb;
		this->distBetweenTwoTrajPoints = distBetweenTwoTrajPoints;
		this->routeNetworkDistBetweenTwoTrajPoints = routeNetworkDistBetweenTwoTrajPoints;
		this->transactionProb = transactionProb;
		/*Ϊ�ٶ�LBS�������޸�*/
	}
};

/*Ϊ�ٶ�LBS�������޸�*/
struct MatchedInfo{
	GeoPoint* formerPoint;
	Edge* formerMatchedEdge;
	GeoPoint* latterPoint;
	Edge* latterMatchedEdge;
	double transactionProb;
	MatchedInfo(GeoPoint* formerPoint, GeoPoint* latterPoint, Edge* formerMatchedEdge, Edge* latterMatchedEdge, double transactionProb){
		this->formerPoint = formerPoint;
		this->latterPoint = latterPoint;
		this->formerMatchedEdge = formerMatchedEdge;
		this->latterMatchedEdge = latterMatchedEdge;
		this->transactionProb = transactionProb;
	}
};
list<MatchedInfo*> matchedInfos;
/*Ϊ�ٶ�LBS�������޸�*/

//������ʼ��㺯��
double EmissionProb(double t, double dist){
	return exp(t*dist * dist * N2_SIGMAZ2) * SQR_2PI_SIGMAZ;
}

//��������������������������scoreMatrix�и���������������ĺ�ѡ·�ε�����
int GetStartColumnIndex(vector<Score> &row){
	int resultIndex = -1;
	long double currentMaxProb = -1;
	for (size_t i = 0; i < row.size(); i++){
		if (currentMaxProb < row.at(i).score){
			currentMaxProb = row.at(i).score;
			resultIndex = i;
		}
	}
	return resultIndex;
}

//������ƥ��·�β����ӣ���������·�μ���������·��ͨ����·��
//list<Edge*> linkMatchedResult(list<Edge*> &mapMatchingResult){
//	list<Edge*> result = list<Edge*>();
//	Edge* lastEdge = NULL;
//	for (list<Edge*>::iterator iter = mapMatchingResult.begin(); iter != mapMatchingResult.end(); iter++)
//	{
//		if (lastEdge == NULL){
//			lastEdge = *iter;
//			result.push_back(*iter);
//			continue;
//		}
//		else{
//			if ((*iter) == NULL){
//				continue;
//			}
//			else{
//				if (lastEdge != *iter&&lastEdge->endNodeId != (*iter)->startNodeId){
//					routeNetwork.shortestPathLength(lastEdge->endNodeId, (*iter)->startNodeId);//TODO�����汾shortestpath������ͬ
//					result.insert(result.end(), shortestPath.begin(), shortestPath.end());
//					result.push_back(*iter);
//					lastEdge = *iter;
//				}
//			}
//		}
//	}
//	return result;
//}

void DrawLowTransationProb(double rate){
	cout << "size��" << matchedInfos.size() << endl;
	matchedInfos.sort([&](const MatchedInfo* m1, const MatchedInfo* m2){
		return (m1->transactionProb) < (m2->transactionProb);
	});
	size_t index = 0;
	for each (MatchedInfo* var in matchedInfos)
	{
		if (index < matchedInfos.size()*rate){
			md.drawBigPoint(Color::Green, var->formerPoint->lat, var->formerPoint->lon);
			md.drawBigPoint(Color::Red, var->latterPoint->lat, var->latterPoint->lon);
			md.drawLine(Color::Red, var->formerPoint->lat, var->formerPoint->lon, var->latterPoint->lat, var->latterPoint->lon);
		}
		index++;
	}
}

list<Edge*> MapMatching(list<GeoPoint*> &trajectory){
	list<Edge*> mapMatchingResult;//ȫ��ƥ��·��
	int sampleRate = (trajectory.back()->time - trajectory.front()->time) / (trajectory.size() - 1);//����켣ƽ��������
	//cout << "�����ʣ�" << sampleRate << endl;
	if (sampleRate > 30){ sampleRate = 30; }
	long double BT = (long double)BETA_ARR[sampleRate];//���ݹ켣ƽ��������ȷ��betaֵ������ת�Ƹ���ʱʹ��
	vector<vector<Score>> scoreMatrix = vector<vector<Score>>();//���й켣��ĸ��ʾ���
	//��Ҫ��ÿ��ѭ������ǰ���µı���
	GeoPoint* formerTrajPoint = NULL;//��һ���켣�㣬����·������ʱ��Ҫ
	bool cutFlag = true;//û��ǰһ�켣���ǰһ�켣��û�д��ĺ�ѡ·��
	int currentTrajPointIndex = 0;//��ǰ�켣�������	
	for (list<GeoPoint*>::iterator trajectoryIterator = trajectory.begin(); trajectoryIterator != trajectory.end(); trajectoryIterator++)//����ÿ���켣��
	{
		double deltaT = -1;//��ǰ��켣�����ʱ��deltaT��ʾǰ�����켣����ʱ���
		if (formerTrajPoint != NULL){ deltaT = (*trajectoryIterator)->time - formerTrajPoint->time; }
		long double currentMaxProb = -1e10;//��ǰ���������ʣ���ʼֵΪ-1e10
		vector<Score> scores = vector<Score>();//��ǰ�켣���Score����
		vector<Edge*> canadidateEdges;//��ѡ·�μ���
		routeNetwork.getNearEdges((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, RANGEOFCANADIDATEEDGES, canadidateEdges);//���������ָ����Χ�ڵĺ�ѡ·�μ���
		long double *emissionProbs = new long double[canadidateEdges.size()];//�����ѡ·�εķ������
		int currentCanadidateEdgeIndex = 0;//��ǰ��ѡ·�ε�����
		for each (Edge* canadidateEdge in canadidateEdges)
		{
			/*Ϊ�ٶ�LBS�������޸�*/
			double distBetweenTrajPointAndEdgeForBaiduLBS = -1;
			double emissionProbForBaiduLBS = -1;
			double distBetweenTrajPointsForBaiduLBS = -1;
			double distBetweenEdgeForBaiduLBS = -1;
			long double transactionProbForBaiduLBS = -1;
			/*Ϊ�ٶ�LBS�������޸�*/
			int preColumnIndex = -1;//���浱ǰ��ѡ·�ε�ǰ��·�ε�������
			double currentDistLeft = 0;//��ǰ�켣���ں�ѡ·���ϵ�ͶӰ���·�����ľ���
			double distBetweenTrajPointAndEdge = routeNetwork.distMFromTransplantFromSRC((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, canadidateEdge, currentDistLeft);
			//������Щ��ѡ·�εķ������
			emissionProbs[currentCanadidateEdgeIndex] = EmissionProb(1, distBetweenTrajPointAndEdge);
			/*Ϊ�ٶ�LBS�������޸�*/
			distBetweenTrajPointAndEdgeForBaiduLBS = distBetweenTrajPointAndEdge;
			emissionProbForBaiduLBS = emissionProbs[currentCanadidateEdgeIndex];
			/*Ϊ�ٶ�LBS�������޸�*/
			if (!cutFlag){
				//��ǰ�����㲻�ǹ켣��һ�����ƥ���жϺ�ĵ�һ���㣬�����ת�Ƹ���
				long double currentMaxProbTmp = -1e10;//��ǰ���ת�Ƹ��ʣ���ʼֵΪ-1e10
				int formerCanadidateEdgeIndex = 0;
				double distBetweenTwoTrajPoints = GeoPoint::distM((*trajectoryIterator)->lat, (*trajectoryIterator)->lon, formerTrajPoint->lat, formerTrajPoint->lon);//���켣����ֱ�Ӿ���
				/*Ϊ�ٶ�LBS�������޸�*/
				distBetweenTrajPointsForBaiduLBS = distBetweenTwoTrajPoints;
				/*Ϊ�ٶ�LBS�������޸�*/
				for each(Score formerCanadidateEdge in scoreMatrix.back()){
					double formerDistLeft = formerCanadidateEdge.distLeft;//ǰһ���켣���ں�ѡ·���ϵ�ͶӰ���·�����ľ���
					double formerDistToEnd = formerCanadidateEdge.edge->lengthM - formerDistLeft;//ǰһ���켣���ں�ѡ·���ϵ�ͶӰ���·���յ�ľ���
					double routeNetworkDistBetweenTwoEdges;//��·������ľ���
					double routeNetworkDistBetweenTwoTrajPoints;//���켣���Ӧ��ͶӰ����·������
					if (canadidateEdge == formerCanadidateEdge.edge){//���ǰһƥ��·�κ͵�ǰ��ѡ·����ͬһ·�Σ������߼���������ĲΪ·������
						routeNetworkDistBetweenTwoTrajPoints = fabs(currentDistLeft - formerCanadidateEdge.distLeft);
					}
					else{
						pair<int, int> odPair = make_pair(formerCanadidateEdge.edge->endNodeId, canadidateEdge->startNodeId);
						//���������յ����·�Ѿ���������Ҳ���INF
						if (shortestDistPair.find(odPair) != shortestDistPair.end() && shortestDistPair[odPair].first < INF){
							//�����ǰdeltaT�µ��ƶ��������ޱ���̾���Ҫ�󣬵������·�����õ���Ҳ�Ǳ���ľ���ֵ����֮�õ��ľ���INF
							shortestDistPair[odPair].first <= MAXSPEED*deltaT ? routeNetworkDistBetweenTwoEdges = shortestDistPair[odPair].first : routeNetworkDistBetweenTwoEdges = INF;
						}
						else{
							if (shortestDistPair.find(odPair) != shortestDistPair.end() && deltaT <= shortestDistPair[odPair].second){//����ĸ��������յ����·�����INF���ҵ�ǰdeltaT���ϴμ������·ʱ���ƶ�ʱ��ҪС��˵����ǰdeltaT�µõ������·��������INF
								routeNetworkDistBetweenTwoEdges = INF;
							}
							else{
								//����δ������������յ�����·��������ߵ�ǰdeltaT�ȱ����deltaTҪ�󣬿��ܵõ����������·�������֮����Ҫ���ú����������·
								list<Edge*> shortestPath = list<Edge*>();
								routeNetworkDistBetweenTwoEdges = routeNetwork.shortestPathLength(formerCanadidateEdge.edge->endNodeId, canadidateEdge->startNodeId, shortestPath, currentDistLeft, formerDistToEnd, deltaT);//TODO
								shortestDistPair[odPair] = make_pair(routeNetworkDistBetweenTwoEdges, deltaT);
							}
						}
						routeNetworkDistBetweenTwoTrajPoints = routeNetworkDistBetweenTwoEdges + currentDistLeft + formerDistToEnd;
					}
					long double transactionProb = exp(-fabs((long double)distBetweenTwoTrajPoints - (long double)routeNetworkDistBetweenTwoTrajPoints) / BT) / BT;//ת�Ƹ���
					/*GIS2012CUP���Ż����ڴ˴�����transactionProb�����޸�*/
					long double tmpTotalProbForTransaction = formerCanadidateEdge.score * transactionProb;
					if (currentMaxProbTmp < tmpTotalProbForTransaction){//������ǰת�Ƹ��ʺ���֪���ת�Ƹ����нϴ���
						currentMaxProbTmp = tmpTotalProbForTransaction;
						preColumnIndex = formerCanadidateEdgeIndex;
						/*Ϊ�ٶ�LBS�������޸�*/
						distBetweenEdgeForBaiduLBS = routeNetworkDistBetweenTwoTrajPoints;
						transactionProbForBaiduLBS = transactionProb;
						/*Ϊ�ٶ�LBS�������޸�*/
					}
					formerCanadidateEdgeIndex++;
				}
				//��ʱ��emissionProbs������Ǻ�ѡ·�εķ�����ʣ�����ת�Ƹ������Ϊ��ѡ·�ε��������
				emissionProbs[currentCanadidateEdgeIndex] *= currentMaxProbTmp;
			}
			/*����Ҫ�������������ٶȣ���ֻ��������ʴ���MINPROB�ĺ�ѡ·�η��뵱ǰ�켣���Score�����У���������к�ѡ·�η���Score������*/
			scores.push_back(Score(canadidateEdge, emissionProbs[currentCanadidateEdgeIndex], preColumnIndex, currentDistLeft, distBetweenTrajPointsForBaiduLBS, distBetweenEdgeForBaiduLBS, transactionProbForBaiduLBS));
			//�õ���ǰ���������ʣ��Ա��һ��
			if (currentMaxProb < emissionProbs[currentCanadidateEdgeIndex]){ currentMaxProb = emissionProbs[currentCanadidateEdgeIndex]; }
			currentCanadidateEdgeIndex++;
		}
		delete[]emissionProbs;
		formerTrajPoint = *trajectoryIterator;
		currentTrajPointIndex++;
		for (size_t i = 0; i < scores.size(); i++)	{ scores[i].score /= currentMaxProb; }//��һ��
		scoreMatrix.push_back(scores);//�Ѹù켣���Scores�������scoreMatrix��
		if (scores.size() == 0){//��scores����Ϊ�գ���˵��û��һ�����ĺ�ѡ·�Σ�cutFlag��Ϊtrue�������켣��Ϊ�¹켣����ƥ��
			cutFlag = true;
			formerTrajPoint = NULL;
		}
		else
		{
			cutFlag = false;
		}
	}

	//�õ�ȫ��ƥ��·��
	int startColumnIndex = GetStartColumnIndex(scoreMatrix.back());//�õ����һ���켣�����scoreMatrix��Ӧ���е÷���ߵ�����������ȫ��ƥ��·�����յ�
	list<GeoPoint*>::reverse_iterator rIter = trajectory.rbegin();
	/*Ϊ�ٶ�LBS�������޸�*/
	//ofstream foutForBaiduLBS = ofstream("prob"+ToString(currentTrajPointIndex)+".txt");
	list<GeoPoint*>::reverse_iterator tmpRIter;
	/*Ϊ�ٶ�LBS�������޸�*/
	for (int i = scoreMatrix.size() - 1; i >= 0; i--, rIter++){
		if (startColumnIndex != -1){
			/*Ϊ�ٶ�LBS�������޸�*/
			//�����ʽ��ǰ��edgeId ��ǰedgeId ǰ�������γ�� ǰ������㾭�� ��ǰ������γ�� ��ǰ�����㾭�� ת�Ƹ���
			list<GeoPoint*>::reverse_iterator tmpRIter = rIter;
			tmpRIter++;
			if (tmpRIter != trajectory.rend()){
				if (scoreMatrix[i][startColumnIndex].preColumnIndex != -1){//preColumnIndex��Ϊ-1����ʾ��һ���������ѡ·�ε���ǰ�������ѡ·������ת�Ƹ��ʵ�
					MatchedInfo *info = new MatchedInfo(*tmpRIter, *rIter, scoreMatrix[i - 1][scoreMatrix[i][startColumnIndex].preColumnIndex].edge, scoreMatrix[i][startColumnIndex].edge, scoreMatrix[i][startColumnIndex].transactionProb);
					matchedInfos.push_front(info);
					//foutForBaiduLBS << scoreMatrix[i][startColumnIndex].preColumnIndex << " " << scoreMatrix[i][startColumnIndex].edge->id << " " << (*tmpRIter)->lat << " " << (*tmpRIter)->lon << " " << (*rIter)->lat << " " << (*rIter)->lon << " " << scoreMatrix[i][startColumnIndex].transactionProb << endl;
				}
			}
			/*Ϊ�ٶ�LBS�������޸�*/

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
	/*Ϊ�ٶ�LBS�������޸�*/
	//foutForBaiduLBS.close();
	/*Ϊ�ٶ�LBS�������޸�*/
	//���Դ��룺������յĸ��ʾ��������ĳ���켣������к�ѡ·�ε�������ʾ�Ϊ��Ϊ����С/������ǿ��ܾͲ���������Ҫ��һ�������и��ʵĵõ�����
	//for (int i = 0; i < scoreMatrix.size(); i++){
	//	logOutput << scoreMatrix.at(i).size() << "\t";
	//	for (int j = 0; j < scoreMatrix.at(i).size(); j++){
	//		logOutput << "[" << scoreMatrix.at(i).at(j).edge->id << "][" << scoreMatrix.at(i).at(j).preColumnIndex << "][" << scoreMatrix.at(i).at(j).score << "]\t";
	//	}
	//	logOutput << endl;
	//}
	//���Խ���

	return mapMatchingResult;
	//return linkMatchedResult(mapMatchingResult);
}

void main(){
	//logOutput = ofstream("debug.txt");
	//logOutput.setf(ios::showpoint);
	//logOutput.precision(8);
	md.setArea(minLat, maxLat, minLon, maxLon);
	md.setResolution(6000);
	routeNetwork.setArea(md);
	BaiduTrajReader fReader(rootFilePath+"traj_hefei.txt");
	fReader.readTrajs(trajList, 1000);
	int trajIndex = 0;
	md.newBitmap();
	md.lockBits();
	routeNetwork.drawMap(Color::Blue, md);
	cout << "��ʼ��ͼƥ�䣡" << endl;
	for (list<Traj*>::iterator trajIter = trajList.begin(); trajIter != trajList.end(); trajIter++){
		list<Edge*> resultList = MapMatching(*(*trajIter));
		//ofstream MatchedEdgeOutput(rootFilePath + "output\\" + outputFileNames[trajIndex]);
		//Traj::iterator trajPointIter = (*trajIter)->begin();
		//for (list<Edge*>::iterator edgeIter = resultList.begin(); edgeIter != resultList.end(); edgeIter++, trajPointIter++){
		//	if (*edgeIter != NULL){
		//		int currentIndex = (*edgeIter)->id;
		//		//MatchedEdgeOutput << (*trajPointIter)->time << "," << currentIndex << ",1.0" << endl;
		//		MatchedEdgeOutput << "0," << currentIndex << ",1.0" << endl;
		//	}
		//	else{
		//		//MatchedEdgeOutput << (*trajPointIter)->time << "," << -1 << ",1.0" << endl;
		//		MatchedEdgeOutput << "0," << -1 << ",1.0" << endl;
		//	}
		//}
		//MatchedEdgeOutput.close();
		//cout << "��" << trajIndex << "���켣ƥ����ϣ�" << endl;
		//trajIndex++;
	}
	DrawLowTransationProb(0.01);
	md.unlockBits();
	md.saveBitmap("pic.png");
	//logOutput.close();
}