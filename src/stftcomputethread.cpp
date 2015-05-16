#include "stftcomputethread.h"

#include "sigproc.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"
#include "ftsound.h"
#include "qthelper.h"
#include "../external/libqxt/qxtspanslider.h"
#include "colormap.h"
#include "ftsound.h"

STFTComputeThread::STFTParameters::STFTParameters(FTSound* reqnd, const std::vector<FFTTYPE>& reqwin, int reqstepsize, int reqdftlen, int reqcepliftorder, bool reqcepliftpresdc){
    clear();

    snd = reqnd;
    ampscale = reqnd->m_ampscale;
    delay = reqnd->m_delay;
    win = reqwin;
    stepsize = reqstepsize;
    dftlen = reqdftlen;
    cepliftorder = reqcepliftorder;
    cepliftpresdc = reqcepliftpresdc;
}

bool STFTComputeThread::STFTParameters::operator==(const STFTParameters& param) const {
    if(snd!=param.snd)
        return false;
    if(ampscale!=param.ampscale)
        return false;
    if(delay!=param.delay)
        return false;
    if(stepsize!=param.stepsize)
        return false;
    if(dftlen!=param.dftlen)
        return false;
    if(cepliftorder!=param.cepliftorder)
        return false;
    if(cepliftpresdc!=param.cepliftpresdc)
        return false;
    if(win.size()!=param.win.size())
        return false;
    for(size_t n=0; n<win.size(); n++)
        if(win[n]!=param.win[n])
            return false;

    return true;
}

STFTComputeThread::STFTComputeThread(QObject* parent)
    : QThread(parent)
{
    m_fft = new sigproc::FFTwrapper();
//    setPriority(QThread::IdlePriority);
}

void STFTComputeThread::compute(ImageParameters reqImgSTFTParams) {

    if(reqImgSTFTParams.stftparams.win.size()<2)
        throw QString("Window's length is too short");

    m_mutex_changingparams.lock();

//    COUTD << "STFTComputeThread::compute winlen=" << winlen << " stepsize=" << stepsize << " dftlen=" << dftlen << std::endl;

    // Check if this is necessary to re-compute the STFT.
    // Maybe updating the image is sufficient.
    reqImgSTFTParams.stftparams.computestft = reqImgSTFTParams.stftparams.snd->m_stftparams.isEmpty()
            || (reqImgSTFTParams.stftparams.snd->m_stftparams!=reqImgSTFTParams.stftparams);

//    COUTD << "Compute STFT " << reqImgSTFTParams.stftparams.computestft << std::endl;
    if(m_mutex_computing.tryLock()) {
        // Currently not computing, so start it!

        gMW->ui->pbSTFTComputingCancel->setChecked(false);
        gMW->ui->pbSTFTComputingCancel->show();

        m_params_current = reqImgSTFTParams;
        start(); // Start computing
    }
    else {
        // Currently already computing something
        // So cancel it and run the new params
        if(reqImgSTFTParams!=m_params_current && reqImgSTFTParams!=m_params_todo) {
            gMW->ui->pbSTFTComputingCancel->setChecked(true);
            m_params_todo = reqImgSTFTParams;  // Ask to compute a new one, once the current computation is finished
        }
    }

//    std::cout << "~STFTComputeThread::compute" << std::endl;

    m_mutex_changingparams.unlock();
}

void STFTComputeThread::run() {
//    COUTD << "STFTComputeThread::run" << std::endl;

    // Prepare some usefull variables
    std::vector<WAVTYPE>::iterator pstft;

    bool canceled = false;
    bool compute = true;
    do{
        // If asked, update the STFT
//        COUTD << m_params_current.stftparams.computestft << std::endl;
        if(m_params_current.stftparams.computestft){
            emit stftComputingStateChanged(SCSDFT);

            m_fft->resize(m_params_current.stftparams.dftlen);

            qreal gain = m_params_current.stftparams.ampscale;

            std::vector<WAVTYPE>* wav = &m_params_current.stftparams.snd->wav;

//        COUTD << "STFTComputeThread::run resize finished" << std::endl;

            int stepsize = m_params_current.stftparams.stepsize;
            int dftlen = m_params_current.stftparams.dftlen;
            std::vector<FFTTYPE>& win = m_params_current.stftparams.win;
            int winlen = win.size();
            int fs = m_params_current.stftparams.snd->fs;
            FFTTYPE stftmin = std::numeric_limits<FFTTYPE>::infinity();
            FFTTYPE stftmax = -std::numeric_limits<FFTTYPE>::infinity();
            qint64 snddelay = m_params_current.stftparams.snd->m_delay;
            std::deque<std::vector<WAVTYPE> >& stft = m_params_current.stftparams.snd->m_stft;

            stft.clear();
            m_params_current.stftparams.snd->m_stftts.clear();

//            COUTD << "INIT: stftmin=" << stftmin << " stftmax=" << stftmax << std::endl;
//            COUTD << "winlen=" << winlen << " dftlen=" << dftlen << "(plan=" << m_fft->size() << ")" << std::endl;

            int maxsampleindex = int(wav->size())-1 + int(m_params_current.stftparams.snd->m_delay);
            WAVTYPE value;

//            QTime starttime = QTime::currentTime();

            for(int si=0; !gMW->ui->pbSTFTComputingCancel->isChecked(); si++){
                if(int(si*stepsize+winlen)-1 > maxsampleindex)
                    break;

                // Add a new frame to the STFT
                stft.push_back(std::vector<WAVTYPE>(dftlen/2+1));
                m_params_current.stftparams.snd->m_stftts.push_back((si*stepsize+(winlen-1)/2.0)/fs);

//                COUTD << "si=" << si << "(" << m_params_current.stftparams.snd->m_stftts.back() << ") " << m_params_current.stftparams.snd->m_stft[si].size() << std::endl;


                // Set the DFT's input
                int n = 0;
                int wn = 0;
                bool hasnonzerovalues = false;
                for(; n<int(win.size()); ++n){
                    wn = si*stepsize+n - snddelay;
                    if(wn>=0 && wn<int(wav->size())) {
                        value = gain*(*(wav))[wn];

                        if(value>1.0)       value = 1.0;
                        else if(value<-1.0) value = -1.0;

                        value *= win[n];

                        if(std::abs(value)>0.0)
                            hasnonzerovalues = true;
//                        else
//                            value = 1e-12; //1000*std::numeric_limits<float>::min();

                        m_fft->setInput(n, value);
                        // m_fft->in[n] = value;
                    }
                    else
                        m_fft->setInput(n, 0.0);
//                        m_fft->in[n] = 0.0;
                }

//                COUTD << si << ": " << hasnonzerovalues << std::endl;

                if(hasnonzerovalues){
                    // Zero-pad the DFT's input
                    for(; n<dftlen; ++n)
                        m_fft->setInput(n, 0.0);
//                        m_fft->in[n] = 0.0;

                    m_fft->execute(false); // Compute the DFT
                    // m_fft->execute(true); // Compute the DFT

                    // Retrieve DFT's output
                    stft[si][0] = std::log(std::abs(m_fft->getDCOutput()));
                    for(n=1; n<dftlen/2; ++n)
                        stft[si][n] = std::log(std::abs(m_fft->getMidOutput(n)));
                    stft[si][dftlen/2] = std::log(std::abs(m_fft->getNyquistOutput()));

//                    for(n=0; n<=m_params_current.stftparams.dftlen/2; n++)
//                        m_params_current.stftparams.snd->m_stft[si][n] = std::log(std::abs(m_fft->out[n]));
//                        m_params_current.stftparams.snd->m_stft[si][n] = std::log(std::abs(m_fft->getMidOutput(n)));

                    if(m_params_current.stftparams.cepliftorder>0){
                        std::vector<FFTTYPE> cc;
                        // First, fix possible Inf amplitudes to avoid ending up with NaNs.
                        if(qIsInf(stft[si][0]))
                            stft[si][0] = stft[si][1];
                        for(int n=1; n<dftlen/2+1; ++n) {
                            if(qIsInf(stft[si][n]))
                                stft[si][n] = stft[si][n-1];
                        }
                        hspec2rcc(stft[si], m_fft, cc);
                        std::vector<FFTTYPE> win = sigproc::hamming(m_params_current.stftparams.cepliftorder*2+1);
                        for(int n=0; n<m_params_current.stftparams.cepliftorder && n<int(cc.size())-1; ++n)
                            cc[n+1] *= win[n];
                        if(!m_params_current.stftparams.cepliftpresdc)
                            cc[0] = 0.0;
                        rcc2hspec(cc, m_fft, stft[si]);
//                        if(si==499) {
//                            std::cout << "Liftered:=";
//                            for(int n=0; n<dftlen/2+1; n++) {
//                                std::cout << stft[si][n] << " ";
//                            }
//                            std::cout << std::endl;
//                        }
                    }

                    // Convert to [dB] and compute min and max magnitudes[dB]
                    for(n=0; n<dftlen/2+1; n++) {
                        FFTTYPE value = sigproc::log2db*stft[si][n];

                        if(std::isnan(value))
                            value = -std::numeric_limits<FFTTYPE>::infinity();

//                        if(value<-300)
//                            COUTD << "si=" << si << " n=" << n << ": " << value << std::endl;

                        stft[si][n] = value;

                        // Do not consider Inf values as well as DC and Nyquist (Too easy to degenerate)
                        if(n!=0 && n!=dftlen/2 && !qIsInf(value)) {
                            stftmin = std::min(stftmin, value);
                            stftmax = std::max(stftmax, value);
                        }
                    }
                }
                else{
                    pstft = stft[si].begin();
                    for(n=0; n<dftlen/2+1; n++, pstft++)
                        *pstft = -std::numeric_limits<FFTTYPE>::infinity();
                }

                emit stftProgressing(int(100*double(si*m_params_current.stftparams.stepsize)/maxsampleindex));
            }

//            COUTD << "stftlen=" << m_params_current.stftparams.snd->m_stft.size() << std::endl;

            if(!gMW->ui->pbSTFTComputingCancel->isChecked()){
                // The STFT is done
                m_mutex_changingparams.lock();

                m_params_current.stftparams.snd->m_stftparams = m_params_current.stftparams;

                if(qIsInf(stftmin) && qIsInf(stftmax)){
                    stftmax = 0.0; // Default 0dB
                    stftmin = -1.0; // Default -1dB
                }
                else if(qIsInf(stftmin))
                    stftmin = stftmax - 1.0;
                else if(qIsInf(stftmax))
                    stftmax = stftmin + 1.0;

                m_params_current.stftparams.snd->m_stft_min = stftmin;
                m_params_current.stftparams.snd->m_stft_max = stftmax;

                m_mutex_changingparams.unlock();
//                COUTD << "stftmin=" << stftmin << " stftmax=" << stftmax << std::endl;
//                COUTD << "stftmin=" << m_params_current.stftparams.snd->m_stft_min << " stftmax=" << m_params_current.stftparams.snd->m_stft_max << std::endl;
            }

//            std::cout << "Spent: " << starttime.elapsed() << std::endl;
        }

        // Update the STFT image
        if(!gMW->ui->pbSTFTComputingCancel->isChecked()){
            emit stftComputingStateChanged(SCSIMG);

//            QTime starttime = QTime::currentTime();

            *(m_params_current.imgstft) = QImage(int(m_params_current.stftparams.snd->m_stft.size()), int(m_params_current.stftparams.snd->m_stft[0].size()), QImage::Format_RGB32);
            m_params_current.imgstft->fill(Qt::black);
            // QImage* img = m_params_current.imgstft;
            QRgb* pimgb = (QRgb*)(m_params_current.imgstft->bits());
            int halfdftlen = m_params_current.stftparams.dftlen/2;
            int dftsize = int(m_params_current.stftparams.snd->m_stft[0].size());
            int stftlen = int(m_params_current.stftparams.snd->m_stft.size());
            bool colormap_reversed = m_params_current.colormap_reversed;

            ColorMap& cmap = ColorMap::getAt(m_params_current.colormap_index);

//            COUTD << "IMG: stftmin=" << m_params_current.stftparams.snd->m_stft_min << "dB stftmax=" << m_params_current.stftparams.snd->m_stft_max << "dB" << std::endl;

            FFTTYPE ymin = m_params_current.stftparams.snd->m_stft_min+(m_params_current.stftparams.snd->m_stft_max-m_params_current.stftparams.snd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->lowerValue()/100.0; // Min of color range [dB]
            FFTTYPE ymax = m_params_current.stftparams.snd->m_stft_min+(m_params_current.stftparams.snd->m_stft_max-m_params_current.stftparams.snd->m_stft_min)*gMW->m_qxtSpectrogramSpanSlider->upperValue()/100.0; // Max of color range [dB]
            FFTTYPE divmaxmmin = 1.0/(ymax-ymin);
//            QRgb red = qRgb(int(255*1), int(255*0), int(255*0));
            QRgb c0 = colormap_reversed?cmap(1.0):cmap(0.0);
            QRgb c1 = colormap_reversed?cmap(0.0):cmap(1.0);
            FFTTYPE y;
            QRgb c;

//            COUTD << "ymin=" << ymin << " ymax=" << ymax << std::endl;
            for(int si=0; si<stftlen && !gMW->ui->pbSTFTComputingCancel->isChecked(); si++, pimgb++){
                pstft = m_params_current.stftparams.snd->m_stft[si].begin();
                for(int n=0; n<dftsize; n++, pstft++) {

                    y = (*pstft-ymin)*divmaxmmin;

                    if(y<=0.0)
                        c = c0;
                    else if(y>=1.0)
                        c = c1;
                    else {
                        if(colormap_reversed)
                            y = 1.0-y;

                        c = cmap(y);
                    }

                    *(pimgb + (halfdftlen-n)*stftlen) = c;
//                    *(pimgb + n*stftlen + si) = c;

                    // img->setPixel(si, n, c); // Bit slower, though can take adv. of hardware optim ?
                }
//                std::cout << si << ": " << y << std::endl;
                emit stftProgressing((100*si)/stftlen);
            }

//            m_params_current.stftparams.snd->m_stft_min = std::max(FFTTYPE(-2.0*20*std::log10(std::pow(2,m_params_current.stftparams.snd->format().sampleSize()))), m_params_current.stftparams.snd->m_stft_min); Why doing this ??
//            COUTD << "Image Spent: " << starttime.elapsed() << std::endl;
        }

        canceled = gMW->ui->pbSTFTComputingCancel->isChecked();
        if(gMW->ui->pbSTFTComputingCancel->isChecked()){
            m_mutex_changingparams.lock();
            if(m_params_current.stftparams.snd->m_stftparams != m_params_current.stftparams) {
                m_params_current.stftparams.snd->m_stft.clear();
                m_params_current.stftparams.snd->m_stftts.clear();
                m_params_current.stftparams.snd->m_stftparams.clear();
            }
            m_mutex_changingparams.unlock();
//            m_params_current.snd->m_stft_lastupdate = QTime();
//            m_params_current.snd->m_stftimg_lastupdate = QTime();
            gMW->ui->pbSTFTComputingCancel->setChecked(false);
        }

//        COUTD << "STFTComputeThread::run check for computing again ..." << std::endl;
        // Check if it has to compute another
        m_mutex_changingparams.lock();
        if(!m_params_todo.isEmpty()){
//            COUTD << "STFTComputeThread::run something to compute again !" << std::endl;

            // m_mutex_computing.unlock();
            m_params_current = m_params_todo;
            m_params_todo.clear();
            m_mutex_changingparams.unlock();
        }
        else{
//            COUTD << "STFTComputeThread::run nothing else to compute" << std::endl;
            // m_mutex_computing.unlock();
            m_params_current.clear();
            m_mutex_changingparams.unlock();
            compute = false;
        }

//        COUTD << "STFTComputeThread::run while ..." << std::endl;
    }
    while(compute);

//    COUTD << "STFTComputeThread::run m_mutex_computing.unlock " << std::endl;
    m_mutex_computing.unlock();

//    COUTD << "STFTComputeThread::run emit " << std::endl;
    emit stftComputingStateChanged(SCSIdle);
    emit stftFinished(canceled);

//    COUTD << "STFTComputeThread::~run" << std::endl;
}
