#include <bits/stdc++.h>
#include <fstream>
#include <mpi.h>
using namespace std;
typedef unsigned long L;
#define BLOCK_LOW(my_rank, comm_sz, T) ((my_rank) * (T) / (comm_sz))
#define BLOCK_HIGH(my_rank, comm_sz, T) (BLOCK_LOW(my_rank + 1, comm_sz, T) - 1)
#define BLOCK_SIZE(my_rank, comm_sz, T) (BLOCK_HIGH(my_rank, comm_sz, T) - BLOCK_LOW(my_rank, comm_sz, T) + 1)
bool check(L res[], int len)
{
    for (int i = 0; i < len - 1; i++)
        if (!(res[i] <= res[i + 1]))
            return false;
    return true;
}
struct data
{
    int stindex; // ���鲢�������
    int index;   // �������е����
    L stvalue;
    data(int st = 0, int id = 0, L stv = 0) : stindex(st), index(id), stvalue(stv) {}
};

bool operator<(const data &One, const data &Two)
{
    return One.stvalue > Two.stvalue;
}
void merge(L *start[], const int length[], const int number, L newArray[], const int newArrayLength)
{
    priority_queue<data> q;
    // ��ÿ�����鲢����ĵ�һ�����������ȶ���
    for (int i = 0; i < number; i++)
        if (length[i] > 0)
            q.push(data(i, 0, start[i][0]));
    int newArrayindex = 0;
    while (!q.empty() && (newArrayindex < newArrayLength))
    {
        // ȡ��С����
        data top_data = q.top();
        q.pop();

        // �����µ����ݼ��뵽���������
        newArray[newArrayindex++] = start[top_data.stindex][top_data.index];

        // ��ȡ�����ݲ������һ��������һ��Ԫ��push�����ȶ���
        if (length[top_data.stindex] > (top_data.index + 1))
        {
            q.push(data(top_data.stindex, top_data.index + 1, start[top_data.stindex][top_data.index + 1]));
        }
    }
}
int main(int argc, char *argv[])
{
    int comm_sz, my_rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    int power = strtol(argv[1], NULL, 10);
    L dataLength = pow(2, power); //2^power����
    ifstream fin("/public/home/shared_dir/psrs_data", ios::binary);

    //���㱾���̼������ݿ��С���Լ���ȡλ��
    L myDataStart = BLOCK_LOW(my_rank, comm_sz, dataLength);
    L myDataLength = BLOCK_SIZE(my_rank, comm_sz, dataLength);
    fin.seekg((myDataStart + 1) * sizeof(L), ios::beg);

    //��ȡ����������
    L *myData = new L[myDataLength];
    for (L i = 0; i < myDataLength; i++)
        fin.read((char *)&myData[i], sizeof(L));
    fin.close();

    //��¼ʱ��
    double startTime, endTime;
    MPI_Barrier(MPI_COMM_WORLD);
    startTime = MPI_Wtime();

    //���򱾽�������
    sort(myData, myData + myDataLength);

    //�����ݽ��еȼ��������Regular samples��ÿ������comm_sz��
    L *regularSamples = new L[comm_sz];
    for (int index = 0; index < comm_sz; index++)
        regularSamples[index] = myData[(index * myDataLength) / comm_sz];

    //������Regular Samples���͸�0�Ž���
    L *gatherRegSam;
    if (my_rank == 0)
        gatherRegSam = new L[comm_sz * comm_sz];
    // �����ֱ�Ϊsendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm
    MPI_Gather(regularSamples, comm_sz, MPI_UNSIGNED_LONG, gatherRegSam, comm_sz, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

    //0�Ž���ִ�й鲢���򣬲������Ԫ
    L *privots = new L[comm_sz];
    if (my_rank == 0)
    {
        // start���ڴ洢gatherRegSam�и�����RegularSamples��ʼ�±�
        L **start = new L *[comm_sz];
        int *length = new int[comm_sz];
        for (int i = 0; i < comm_sz; i++)
        {
            start[i] = &gatherRegSam[i * comm_sz];
            length[i] = comm_sz;
        }

        //�鲢
        L *sortedGatRegSam = new L[comm_sz * comm_sz];
        merge(start, length, comm_sz, sortedGatRegSam, comm_sz * comm_sz);

        //ȡ����Ԫ
        for (int i = 0; i < comm_sz - 1; i++)
            privots[i] = sortedGatRegSam[(i + 1) * comm_sz];

        delete[] start;
        delete[] length;
        delete[] sortedGatRegSam;
    }

    //�㲥��Ԫ
    MPI_Bcast(privots, comm_sz - 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

    //�����̰���Ԫ�ֶ�
    int *partStartIndex = new int[comm_sz];
    int *partLength = new int[comm_sz];
    unsigned long dataIndex = 0;
    for (int partIndex = 0; partIndex < comm_sz - 1; partIndex++)
    {
        partStartIndex[partIndex] = dataIndex;
        partLength[partIndex] = 0;

        while ((dataIndex < myDataLength) && (myData[dataIndex] <= privots[partIndex]))
        {
            dataIndex++;
            partLength[partIndex]++;
        }
    }
    partStartIndex[comm_sz - 1] = dataIndex;
    partLength[comm_sz - 1] = myDataLength - dataIndex;

    //ȫ����(ALLTOALL)
    int *recvRankPartLen = new int[comm_sz];
    MPI_Alltoall(partLength, 1, MPI_INT, recvRankPartLen, 1, MPI_INT, MPI_COMM_WORLD);

    //ALLTOALLV
    int rankPartLenSum = 0;
    int *rankPartStart = new int[comm_sz];
    for (int i = 0; i < comm_sz; i++)
    {
        rankPartStart[i] = rankPartLenSum;
        rankPartLenSum += recvRankPartLen[i];
    }
    // ���ո�����i�ε�����
    L *recvPartData = new L[rankPartLenSum];
    MPI_Alltoallv(myData, partLength, partStartIndex, MPI_UNSIGNED_LONG,
                  recvPartData, recvRankPartLen, rankPartStart, MPI_UNSIGNED_LONG, MPI_COMM_WORLD);

    // �鲢
    L **mulPartStart = new L *[comm_sz];
    for (int i = 0; i < comm_sz; i++)
        mulPartStart[i] = &recvPartData[rankPartStart[i]];

    // ���
    L *sortedRes = new L[rankPartLenSum];
    merge(mulPartStart, recvRankPartLen, comm_sz, sortedRes, rankPartLenSum);

    MPI_Barrier(MPI_COMM_WORLD);
    endTime = MPI_Wtime();

    //�ж��Ƿ��Ѿ�����
    cout << "Rank " << my_rank << " sort: " << check(sortedRes, rankPartLenSum) << endl;

    if (my_rank == 0)
    {
        cout << "Processors number :" << comm_sz << " Power:" << power;
        cout << " Time :" << endTime - startTime << endl;
    }
    // �����
    delete[] myData;
    delete[] regularSamples;
    if (my_rank == 0)
        delete[] gatherRegSam;
    delete[] partStartIndex;
    delete[] partLength;
    delete[] recvRankPartLen;
    delete[] rankPartStart;
    delete[] recvPartData;
    delete[] mulPartStart;
    delete[] sortedRes;
    MPI_Finalize();

    return 0;
}

