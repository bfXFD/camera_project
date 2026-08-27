#include <stdio.h>
#include "IRCUSBSDK.h" 
#include "IRCUSBSDKDef.h" // 请确保包含SDK头文件


int deviceNum = 0;       // 存储搜索到的设备ID
int deviceId[100];

// 设备搜索回调函数（注意调用约定需与SDK一致）
void IRC_USB_CALL SearchCallback(IRC_USB_DEV_INFO* dev_info, void* userData)
{
    if (dev_info)
    {
        deviceId[deviceNum++] = dev_info->id;
    }
}
void IRC_USB_CALL FrameCallback(IRC_USB_HANDLE handle, IRC_USB_VIDEO_INFO_CB* videoInfo, void* userData)
{
    //printf("width:%d, height:%d\n", videoInfo->width, videoInfo->height);
}

void IRC_USB_CALL TempCallback(IRC_USB_HANDLE handle, IRC_USB_TEMP_INFO_CB* tempInfo, void* userData)
{
    //printf("width:%d, height:%d\n", tempInfo->width, tempInfo->height);
}

int ReadCoreType(IRC_USB_HANDLE handle)
{
    unsigned char szCmd[0x08] =
    {
        0xAA, 0x04, 0x01, 0x70, 0x00, 0x1F, 0xEB, 0xAA
    };

    int ret = IRC_USB_WritePort(handle, (char*)szCmd, 8);
    if(ret < 0)
    {
        return ret;
    }

    char recvBuf[512] = { 0 };
    int recvCnt = 0;
    ret = IRC_USB_ReadPort(handle, recvBuf, 512, &recvCnt, 1000);
    if (ret != 0)
    {
        return ret;
    }
}

int main() {
    int ret;
    
    IRC_USB_HANDLE handle;    // 设备操作句柄

    // 1. 初始化SDK
    ret = IRC_USB_Init();
    if (ret != 0) {
        printf("初始化失败，错误码: %d\n", ret);
        return -1;
    }

    // 2. 搜索设备（回调函数实时返回设备信息）
    ret = IRC_USB_SearchDev(SearchCallback, NULL);
    if (ret != 0)
    {
        printf("设备搜索失败，错误码: %d\n", ret);
        IRC_USB_Deinit();
        return -1;
    }

    if (deviceNum == 0)
    {
        printf("未找到任何设备\n");
        IRC_USB_Deinit();
        return -1;
    }

    // 3. 打开设备
    printf("open dev start\n");
    ret = IRC_USB_OpenDev(deviceId[0], &handle);
    if (ret != 0 || handle == NULL)
    {
        printf("设备打开失败，错误码: %d\n", ret);
        IRC_USB_Deinit();
        return -1;
    }
    printf("设备已成功打开，句柄: %p\n", handle);

    // 4. 此处可添加设备操作代码...


    ret = IRC_USB_StartPreview(handle, FrameCallback, NULL);
    if (ret != 0)
    {
        printf("预览失败: %d\n", ret);
        IRC_USB_StopPreview(handle);
    }

    ret = IRC_USB_StartPullTemp(handle, TempCallback, NULL);
    if (ret != 0)
    {
        printf("拉取温度失败: %d\n", ret);
        IRC_USB_StopPullTemp(handle);
    }
    char input;
    scanf("%c", &input);
    ReadCoreType(handle);
    IRC_USB_StopPreview(handle);
    IRC_USB_StopPullTemp(handle);

    // 5. 关闭设备
    ret = IRC_USB_CloseDev(handle);
    if (ret != 0)
    {
        printf("设备关闭失败，错误码: %d\n", ret);
    }

    // 6. 反初始化SDK
    IRC_USB_Deinit();

    return 0;
}
