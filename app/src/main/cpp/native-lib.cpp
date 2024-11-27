#include <jni.h>
#include <string>
#include <chrono> // Para calcular el tiempo
#include <android/log.h>

// Define un tag para los logs
#define LOG_TAG "NativeCode"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)


using namespace std;


extern "C" JNIEXPORT jstring JNICALL
Java_ec_edu_ups_interciclovisioncomputador_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/video.hpp>
#include "android/bitmap.h"
#include <vector>


void bitmapToMat(JNIEnv * env, jobject bitmap, cv::Mat &dst, jboolean
needUnPremultiplyAlpha){
    AndroidBitmapInfo info;
    void*
            pixels = 0;try {
        CV_Assert( AndroidBitmap_getInfo(env, bitmap, &info) >= 0 );
        CV_Assert( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 ||
                   info.format == ANDROID_BITMAP_FORMAT_RGB_565 );
        CV_Assert( AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0 );
        CV_Assert( pixels );
        dst.create(info.height, info.width, CV_8UC4);
        if( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 )
        {
            cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
            if(needUnPremultiplyAlpha) cvtColor(tmp, dst, cv::COLOR_mRGBA2RGBA);
            else tmp.copyTo(dst);
        } else {
// info.format == ANDROID_BITMAP_FORMAT_RGB_565
            cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
            cvtColor(tmp, dst, cv::COLOR_BGR5652RGBA);
        }
        AndroidBitmap_unlockPixels(env, bitmap);
        return;
    } catch(const cv::Exception& e) {
        AndroidBitmap_unlockPixels(env, bitmap);
//jclass je = env->FindClass("org/opencv/core/CvException");
        jclass je = env->FindClass("java/lang/Exception");
//if(!je) je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
        return;
    } catch (...) {
        AndroidBitmap_unlockPixels(env, bitmap);
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code {nBitmapToMat}");
        return;
    }
}

void matToBitmap(JNIEnv * env, cv::Mat src, jobject bitmap, jboolean needPremultiplyAlpha) {
    AndroidBitmapInfo info;
    void*
            pixels = 0;
    try {
        CV_Assert( AndroidBitmap_getInfo(env, bitmap, &info) >= 0 );
        CV_Assert( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 ||
                   info.format == ANDROID_BITMAP_FORMAT_RGB_565 );
        CV_Assert( src.dims == 2 && info.height == (uint32_t)src.rows && info.width ==
                                                                         (uint32_t)src.cols );
        CV_Assert( src.type() == CV_8UC1 || src.type() == CV_8UC3 || src.type() == CV_8UC4 );
        CV_Assert( AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0 );
        CV_Assert( pixels );
        if( info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 )
        {
            cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
            if(src.type() == CV_8UC1)
            {
                cvtColor(src, tmp, cv::COLOR_GRAY2RGBA);
            } else if(src.type() == CV_8UC3){cvtColor(src, tmp, cv::COLOR_RGB2RGBA);
            } else if(src.type() == CV_8UC4){
                if(needPremultiplyAlpha) cvtColor(src, tmp, cv::COLOR_RGBA2mRGBA);
                else src.copyTo(tmp);
            }
        } else {
// info.format == ANDROID_BITMAP_FORMAT_RGB_565
            cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
            if(src.type() == CV_8UC1)
            {
                cvtColor(src, tmp, cv::COLOR_GRAY2BGR565);
            } else if(src.type() == CV_8UC3){
                cvtColor(src, tmp, cv::COLOR_RGB2BGR565);
            } else if(src.type() == CV_8UC4){
                cvtColor(src, tmp, cv::COLOR_RGBA2BGR565);
            }
        }
        AndroidBitmap_unlockPixels(env, bitmap);
        return;
    } catch(const cv::Exception& e) {
        AndroidBitmap_unlockPixels(env, bitmap);
//jclass je = env->FindClass("org/opencv/core/CvException");
        jclass je = env->FindClass("java/lang/Exception");
//if(!je) je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
        return;
    } catch (...) {
        AndroidBitmap_unlockPixels(env, bitmap);
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code {nMatToBitmap}");
        return;
    }
}

cv::Mat mask; // Se crea una sola máscara reutilizable

int cannyThreshold1;
int cannyThreshold2;
int skip;  // Controla la densidad de puntos
double scaleFactor = 0.35;  // Escala para redimensionar la imagen

// Estructura de Triángulo
struct Triangle {
    cv::Point2f p1, p2, p3;
    Triangle(cv::Point2f a, cv::Point2f b, cv::Point2f c) : p1(a), p2(b), p3(c) {}
};

bool isPointInCircumcircle(cv::Point2f p, const Triangle& t) {
    double ax = t.p1.x - p.x;
    double ay = t.p1.y - p.y;
    double bx = t.p2.x - p.x;
    double by = t.p2.y - p.y;
    double cx = t.p3.x - p.x;
    double cy = t.p3.y - p.y;

    double det = (ax * ax + ay * ay) * (bx * cy - by * cx) -
                 (bx * bx + by * by) * (ax * cy - ay * cx) +
                 (cx * cx + cy * cy) * (ax * by - ay * bx);

    return det > 0;
}

// Inserta un nuevo punto en la triangulación y actualiza los triángulos
vector<Triangle> insertPoint(vector<Triangle>& triangles, cv::Point2f newPoint) {
    vector<Triangle> newTriangles;
    vector<pair<cv::Point2f, cv::Point2f>> edges;

    for (const auto& t : triangles) {
        if (isPointInCircumcircle(newPoint, t)) {
            edges.push_back({t.p1, t.p2});
            edges.push_back({t.p2, t.p3});
            edges.push_back({t.p3, t.p1});
        } else {
            newTriangles.push_back(t);
        }
    }

    // Eliminar bordes duplicados
    for (size_t i = 0; i < edges.size(); i++) {
        for (size_t j = i + 1; j < edges.size(); j++) {
            if ((edges[i].first == edges[j].second && edges[i].second == edges[j].first) ||
                (edges[i].first == edges[j].first && edges[i].second == edges[j].second)) {
                edges[i] = edges[j] = {cv::Point2f(-1, -1), cv::Point2f(-1, -1)}; // Marcar como duplicados
            }
        }
    }

    // Crear nuevos triángulos con los bordes restantes
    for (const auto& e : edges) {
        if (e.first != cv::Point2f(-1, -1) && e.second != cv::Point2f(-1, -1)) {
            newTriangles.push_back(Triangle(e.first, e.second, newPoint));
        }
    }

    return newTriangles;
}

// Aplica el efecto Low Poly a una imagen
void applyLowPolyEffect(const cv::Mat& img, cv::Mat& output, cv::Mat& edges) {
    cv::Mat resizedImg;
    resize(img, resizedImg, cv::Size(), scaleFactor, scaleFactor);

    Canny(resizedImg, edges, cannyThreshold1, cannyThreshold2);

    vector<cv::Point2f> points;
    for (int y = 0; y < edges.rows; y += skip) {
        for (int x = 0; x < edges.cols; x += skip) {
            if (edges.at<uchar>(y, x) > 0) points.push_back(cv::Point2f(x, y));
        }
    }

    cv::Mat localPoints = cv::Mat::zeros(resizedImg.size(), CV_8UC3); // Cambiar a CV_8UC3 para colores


    vector<Triangle> triangles = {
            Triangle(cv::Point2f(0, 0), cv::Point2f(resizedImg.cols - 1, 0), cv::Point2f(0, resizedImg.rows - 1)),
            Triangle(cv::Point2f(resizedImg.cols - 1, 0), cv::Point2f(resizedImg.cols - 1, resizedImg.rows - 1), cv::Point2f(0, resizedImg.rows - 1))
    };

    // Insertar puntos
    for (const cv::Point2f& p : points) {
        triangles = insertPoint(triangles, p);
    }

    for (const auto& t : triangles) {
        // Convierte a enteros directamente al definir el triángulo
        std::array<cv::Point, 3> triangleInt = {
                cv::Point(cvRound(t.p1.x), cvRound(t.p1.y)),
                cv::Point(cvRound(t.p2.x), cvRound(t.p2.y)),
                cv::Point(cvRound(t.p3.x), cvRound(t.p3.y))
        };

        // Calcula el bounding box del triángulo para limitar la región
        cv::Rect boundingBox = boundingRect(triangleInt);

        // Define la máscara local para el área mínima
        mask = cv::Mat::zeros(boundingBox.size(), CV_8UC1);

        // Ajusta las coordenadas del triángulo al ROI
        std::array<cv::Point, 3> roiTriangle = {
                triangleInt[0] - boundingBox.tl(),
                triangleInt[1] - boundingBox.tl(),
                triangleInt[2] - boundingBox.tl()
        };

        // Rellena el triángulo en la máscara dentro del ROI
        fillConvexPoly(mask, roiTriangle, 255);

        // Calcula el color promedio usando la máscara y el ROI
        cv::Mat roiImg = resizedImg(boundingBox);
        cv::Scalar meanColor = cv::mean(roiImg, mask);

        // Rellena el triángulo en la imagen de salida
        fillConvexPoly(localPoints, triangleInt,
                       cv::Scalar(meanColor[0], meanColor[1], meanColor[2]), cv::LINE_AA);
    }

    resize(localPoints, output, img.size(), 0, 0, cv::INTER_LINEAR);
}

extern "C"
JNIEXPORT void JNICALL
Java_ec_edu_ups_interciclovisioncomputador_MainActivity_procesarFrameEnC(JNIEnv *env, jobject thiz,
                                                                         jobject bitmap_in, jobject bitmap_out, jint stepSize,
                                                                         jint threshold1, jint threshold2) {
    skip = stepSize;
    cannyThreshold1 = threshold1;
    cannyThreshold2 = threshold2;

    static auto last_time = std::chrono::high_resolution_clock::now();

    // Convertir los bitmaps de entrada y salida a Mat
    cv::Mat src, output, edges;
    bitmapToMat(env, bitmap_in, src, false);

    // Aplicar el efecto Low Poly
    applyLowPolyEffect(src, output, edges);

    // Calcular los FPS
    auto current_time = std::chrono::high_resolution_clock::now();
    double fps = 1.0 / std::chrono::duration<double>(current_time - last_time).count();
    last_time = current_time;

    // Dibujar los FPS en la imagen
    std::string fps_text = "FPS: " + std::to_string(static_cast<int>(fps));
    cv::putText(output, fps_text, cv::Point(10, 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 255, 0), 2);

    // Convertir el resultado de nuevo a Bitmap para mostrarlo en Android
    matToBitmap(env, output, bitmap_out, false);
}
