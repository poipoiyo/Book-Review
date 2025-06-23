Heap Sort 的實作主要分為 兩個步驟：
- 建立最大堆（Max Heap）: 把陣列轉換成最大堆，使得每個父節點的值都大於或等於其子節點。
- 排序（Heap Sort）: 逐步將堆頂元素（最大值）與最後一個元素交換，然後對剩下的元素進行堆調整。
C++ Heap Sort 實作
以下是 Heap Sort 的 C++ 版本：
#include <iostream>
using namespace std;

// 函數: 調整為最大堆
void heapify(int arr[], int n, int i) {
    int largest = i; // 初始化最大值為根節點
    int left = 2 * i + 1; // 左子節點
    int right = 2 * i + 2; // 右子節點

    // 如果左子節點存在且大於根節點
    if (left < n && arr[left] > arr[largest])
        largest = left;

    // 如果右子節點存在且大於目前的最大值
    if (right < n && arr[right] > arr[largest])
        largest = right;

    // 如果最大值不是根節點，則交換並遞迴調整
    if (largest != i) {
        swap(arr[i], arr[largest]);
        heapify(arr, n, largest);
    }
}

// 函數: Heap Sort 排序
void heapSort(int arr[], int n) {
    // 建立最大堆
    for (int i = n / 2 - 1; i >= 0; i--)
        heapify(arr, n, i);

    // 逐步排序
    for (int i = n - 1; i > 0; i--) {
        swap(arr[0], arr[i]); // 把最大值移到最後
        heapify(arr, i, 0); // 重新調整堆
    }
}

// 測試 Heap Sort
int main() {
    int arr[] = {10, 20, 15, 30, 40};
    int n = sizeof(arr) / sizeof(arr[0]);

    heapSort(arr, n);

    cout << "排序後的陣列: ";
    for (int i = 0; i < n; i++)
        cout << arr[i] << " ";
    
    return 0;
}


解釋
- heapify() 用於調整最大堆，使得父節點比子節點大。
- heapSort() 先建立最大堆，然後每次把堆頂元素（最大值）移到最後，並調整剩餘的部分。
- main() 創建一個數組，並用 Heap Sort 進行排序，最後輸出結果。
這個方法保證 時間複雜度為 ( O(n \log n) )，適用於許多應用場景。試試看，看看 Heap Sort 在你的數據集上運作如何！ 🚀
