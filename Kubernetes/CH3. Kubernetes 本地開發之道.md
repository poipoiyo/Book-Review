# 第三章 Kubernetes 本地開發之道

符合下列需求，可能會需要創建一個本地的Kubernetes叢集

1. 開發的應用程式與Kubernetes息息相關，會用到它的API
2. 應用程式需要用到一些Kubernetes的資源才能夠看得出差異，例如想確認Kubernetes HPA發生時應用程式是否能夠如期運作
3. 開發人員本身是公司的基礎設施維運人員，需要先在本地進行相關測試
4. 開發程式有很多依賴性，例如需要Redis, Kafka, Memcached



解決方案特性：

1. 容易設定和架設，簡單幾個按鈕
2. 都用指令而不需要UI
3. 模擬多節點的Kubernetes
4. 把上述包成一個腳本，一個命令建置完成



#### KIND (Kubernetes In Docker)

把Kubernetes的節點用Docker的方式來進行，每一個Docker Container就是一個Kubernetes節點，可以當Worker和Master。使用KIND指令搭配一個設定檔案就可以建立Kubernetes叢集。但僅侷限於測試用途，因為所有節點都是基於同機器上面的container，因此效能可能會有所限制。



#### K3D (K3S in Docker)

輕量的Kubernetes版本，適合用在低運算資源系統上。K3D直接將K3S移植到Docker中，讓使用者可以更方便創驗一個叢集。和KIND架構雷同，區別在於K3D是基於K3S。



### 3.1 K3D與KIND的部署示範

#### 3.1.1 K3D : https://github.com/rancher/k3d

安裝都是基於k3d的指令，安裝後創建，可以透過-s調整節點數量，完成後使用docker指令觀察資訊

```
$ sudo curl -s https:raw.githubusercontent.com/rancher/k3d/main
$ k3d cluster
$ k3d cluster create -s 3
$ docker ps
```

Kubernetes的存取、產生新檔案並簡單測試存取、動態新增節點

```
$ k3d kubeconfig
$ k3d kubeconfig merge
$ KUBECONFIG=~/.k3d/kubeconfig-k3s-default.yaml kubectl get nodes

$ k3d node create --role server hwchiu-test
$ k3d node list
$ KUBECONFIG=~/.k3d/kubeconfig-k3s-default.yaml kubectl get nodes
```



#### 3.1.2 KIND : https://github.com/kubernetes-sigs/kind

測試上最常用到的指令為create, delete, load，前兩者專注於Cluster的管理，後者可以將本地的container image複製到Kubernetes節點中。

準備好檔案kind.yaml，可以創造多節點cluster

```yaml
kind: Cluster
apiVersion: kind.sigs.k8s.io/v1alpha3
nodes:
- roles: control-plane
- roles: worker
- roler: worker
```

```
$ kind create cluster --config kind.yaml
```

不同於K3D，KIND本身無法動態增加節點，對於本地測試用，有問題直接砍掉重建即可



### 3.2 本地開發Kubernetes應用程式流程

開發與測試流程：

1. 修改應用程式原始碼
2. 藉助Dockerfile的方式產生一個Docker Container Image
3. 透過原生YAML或是Helm來部署上述產生的Container Image到Kubernetes叢集內
4. Kubernetes叢集根據設定去抓取Container Image並且運行(最關鍵，影響工作效率)

最簡單的做法是完成Image後，直接將它推給準備好的Container Registry，例如Docker Hub。但時間上，Image都要上Docker Hub，Kubernetes在叢Docker Hub去下載剛剛上傳的檔案，時間會根據Image大小和寬頻決定，會降低開發者工作效率。



#### Kubeadm

如果用kubeadm部署，會變得非常簡單，因為Kubeadm預設創立單節點的Kubernetes叢集，只要Kubeadm和開發環境同作業系統，整個檔案系統是共用。開發者只要處理步驟3，修改YAML內描述Image名稱而已。如果要建立多節點的叢集，那大部分節點都沒辦法直接存取開發人員的Image，問題依然存在。



#### KIND/K3D

基於docker節點數部的Kubernetes叢集，架構完全不同。每次部署運算資源時，都會嘗試從本地節點上，也就是Docker Container上去尋找相關的Container Image。

如果有辦法將本地產生的Container Image複製到做為節點的Docker Container上，有辦法縮短等待時間。而該節點鐘則是使用containerd作為其容器解決方案，而不是Docker，沒辦法透過直接掛載的方式來分享Docker Image。



### 3.3 Skaffold 本地開發與測試

基本特性：

1. 指令列工具，架構簡單，不複雜
2. 針對Kubernetes應用程式的開發流程，能夠達到持續開發不被中斷的特性
3. 開發流程包含許多步驟，包含建置、更新、部署
4. 可以和CI/CD pipeline整合
5. 開發人員可以專注於開發，不受Kubernetes影響

可以自動建置Container Image、自動更新Container Image、自動修改YAML裡面的欄位並且更新到Kubernetes叢集



七大功能：

1. Detecting Source Code：內建偵測系統，如果有程式碼更動，就會自動執行相關流程，使用者只要存檔即可
2. File Sync：如我西改只是需要將相關檔案複製到Kubernetes Pod內，則不需要重開Pod，Skaffold會在本地打包並且發送到遠端
3. Building Artifacts：偵測程式碼更動後，會開始進行建置步驟，支援Dockerfile, Bazel, Maven或自定義腳本
4. Test Artifacts：使用框架container-structure-tests進行測試，針對所有建置完畢但未部署到遠方的產物，如果失敗則不會部署
5. Tagging Artifacts：如果通過測試後的產物是Container Image，會將其打上專屬的Tag，預設用Git commit IDS來設定
6. Pushing Artifacts：Container Image準備完畢後，要將其傳送到叢集內，會根據不同軟體建置，選擇不同的傳送方式
7. Deploying Artifacts：將應用程式更新到叢集內，目前支援kubectl, helm, kustomize



#### 3.3.2 Skaffold 安裝與使用

要注意下載完畢後的檔案名稱以及相關權限設定。為了有效控制與設定前面提到的七個功能，需要透過YAML的設定檔案來描述我們期望的設定檔案，並不是七個面向都需要設定。



