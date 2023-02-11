# 第五章 CD Pipeline 與 Kubernetes 探討

討論面向：

1. 使用Pipeline系統加上客製化腳本還是用專門的方式處理CD
2. 不同需求時，部署策略有何不同
3. Kubernetes該套用何種工具管理，不同環境的客製化參數要如何自動化處理
4. 整合CD過程又該如何維持系統的安全性



### 5.1 Pipeline CD 過程思路探討

2020 CNCF使用者雷達CD調查報告

|                            | 專案                                                         |
| -------------------------- | ------------------------------------------------------------ |
| More Actions使用於生產環境 | Flux, Helm                                                   |
| 評估使用中                 | CircleCI, Kustomize, GitLab                                  |
| 評估後保持觀望             | Jsonnet, Jenkins, Travis CI, Tekton CD, GitHub Action, TeamCity, Spinnaker, Jenkins X, Argo CD |

Kubernetes應用程式主流還是Helm，再來是Kustomize。Helm的好處是服務的散播，讓團隊輕鬆地使用來自世界各地開發者所開發的服務，類似apt-get的機制與概念來安裝。而Kustomize透過不同的方式來客製化應用程式，避免Template帶來的複雜性，同時與kubectl工具的整合更是讓Kustomize更加簡單。



更新版本的流派：

1. Recreate：舊版本全部移除乾淨後再重新部署新版本應用程式
2. Ramped：透過one by one的替換策略，每次都會部署一個新版本的實體，透過load-balanced確認該新版本實體可以接受到網路流量且正常運作後，就把舊版本移除，直到舊版本移除
3. Blue/Green：相對於Ramped，藍綠部署一口氣部署新版本所需要的資源，部署完畢且測試完成後，將所有流量導向新版本，移除舊版本
4. Canary(金絲雀)：強調逐步切換的概念，一口氣部署全部的新版本，透過load-balanced的方式慢慢將流量從舊版本導向新版本
5. A/B testing：基於商業上的判斷，針對不同使用者給予不同介面，例如每次Facebook每次升級新版本時，就會有一部份的使用者開始使用新介面，剩下與金絲雀相同
6. Shadow：針對所有流向舊版本的流量都複製一份，將其送到新版本去跑，沒有問題後再移除舊版本



### 5.2 CD 與 Kubernetes 的整合

#### CI 與 CD 整合於相同 Pipeline 工作流程

設計上顯得直覺且簡單，當CI過程結束確認一切測試完畢後就可以直接透過工具(helm)直接更新遠方的Kubernetes。潛在議題是如果未來因為需求想要重新部署應用程式或退板等需求時，整個部署時間可能因為CI時間太長造成部署過久。或是團隊沒有信心，一開始可能會希望先打造半自動工作流程讓CI採全自動化，部署希望有人為手動確認，沒問題才更新。



#### CI 與 CD 分開處理

更常見的做法，較大的靈活與彈性。團隊如果沒有信心，會先採取部署流程自動化，但是整個觸發機制是人為手動的，準備就緒時後才會觸發CD流程。

當團隊信心足夠時，就可以將觸發機制升級成自動觸發，例如跟Git專案整合，當YAML內的檔案都更新成最新狀態後，就自動觸發CD去完成部署。



#### CI 和 CD 分開處理，使用專門軟體處理 CD 階段

不會從Pipeline系統中主動將新版應用程式推到Kubernetes叢集中，而是Kubernetes內會有一個獨立的應用程式，稱為Controller，會自己去判斷是否要更新應用程式。

此種架構不需要CD Pipeline來維護這些事情，因為沒有主動和Kubernetes叢集溝通的需求，所以也不需要把KUBECONFIG放到CD Pipeline系統中，減少安全性隱憂。

整個部署流程都必須依賴Controller的邏輯來處理，如果有任何客製化需求需要確認Controller是否支援，不支援就要自行修改開源軟體或是等對方更新。

相較於完全使用CD Pipeline，這種架構彈性較低，擴充性也較低，框架會被侷限在Controller本身。



### 5.3 以Keel示範如何部署更新Kubernetes

KeeL：

1. 打造一個簡單、強韌的背景服務，能夠自動更新Kubernetes上的運作資源
2. 透過Keel幫忙，開發者不需要去處理如何更新Kubernetes

Keel專案專門設計用來處理自動部署的部分，不需要準備Pipeline系統，Keel會在Kubernetes叢集內安裝一個controller，他會根據規則決定是否要幫應用程式更新。



運作方式1：

1. 修改程式碼並將修改合併到Git
2. 透過Pipeline系統中的CI來處理Container Image，建置後推到遠方的Container Registry
3. Keel於Kubernetes內安裝的Controller會定期去確認Registry的狀態，觀察到新版本出現就會準備相關部署資源。
4. 最後Keel修改Kubernetes內的資源狀態，讓其使用新版本的Image，進而達到更新應用程式



運作方式2：

1. 開發人員修改程式碼並合併到Git
2. 透過Pipeline系統中的CI來處理Container Image，建置後推到遠方的Container Registry
3. Registry知道有新版本厚，透過webhook的方式送往Webhook Relay
4. Keel於Kubernetes內安裝的controller收到來自Webhook Relay送來的更新訊息，從中知道Container Image有新版本的更新，接著準備相關資源準備更新
5. 最後Keel修改Kubernetes內的資源狀態，讓其使用新版本的Container Image



#### Polling

Polling模式大部分情況下會比Webhook來得方便，但是要特別注意可能有流量和次數限制，除了使用方式不同以外，更新速度也會有差異。Webhook更來得及時，Polling要等間隔期滿去詢問才會發覺到新版本出現。



#### Keel 示範

1. 透過Deployment部署事先準備的應用程式
2. 告知Keel幫忙監控該應用程式，有任何新版自動更新
3. 手動更新Container Image，並且更新到遠方的Registry
4. 觀察Keel的Log以及Kubernetes狀況，確認container有更新



