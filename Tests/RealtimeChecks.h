#pragma once
#include "audio/RealtimeAsset.h"
#include "audio/FilePreviewPlayer.h"
#include "dsp/CoverageTracker.h"
#include <thread>

inline void reliabilityChecks()
{
    ml::AnalysisEngine invalidEngine; ml::Stats invalidStats;
    ml::resetStats(invalidStats);invalidEngine.prepare(48000,2);
    juce::AudioBuffer<float> invalidBlock(2,4800);invalidBlock.clear();
    invalidBlock.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
    invalidBlock.setSample(1,1,std::numeric_limits<float>::infinity());
    invalidEngine.process(invalidBlock,invalidStats,4800);invalidEngine.finish(invalidStats);
    unsigned engineNews=0,engineDeletes=0;
    {rtAudit::Scope audit;invalidEngine.process(invalidBlock,invalidStats,4800);engineNews=rtAudit::allocations;engineDeletes=rtAudit::releases;}
    check("engine callback new",engineNews,0,0);check("engine callback delete",engineDeletes,0,0);
    check("nonfinite measurement flagged",ml::snapshot(invalidStats).invalidSamples,4,0);
    check("analysis preserves input NaN",std::isnan(invalidBlock.getSample(0,0)),1,0);
    {
        ml::AnalysisEngine burst;ml::Stats stats;ml::resetStats(stats);burst.prepare(48000,2);
        juce::AudioBuffer<float> audio(2,48000);audio.clear();
        for(int i=3600;i<22800;++i)for(int c=0;c<2;++c)
            audio.setSample(c,i,std::pow(10.0f,-23.0f/20.0f)*std::sin(2.0*juce::MathConstants<double>::pi*997.0*i/48000.0));
        burst.process(audio,stats,48000);burst.finish(stats);
        check("offset burst M peak",ml::snapshot(stats).maxMomentary,-23,.1);
    }
    ml::CoverageTracker coverage;
    coverage.add(.05,.10); coverage.add(0,.05);
    check("coverage fractional union",coverage.seconds(.1),.1,1e-9);
    coverage.add(0,.1);check("coverage loop not doubled",coverage.seconds(1),.1,1e-9);
    coverage.add(.2,.5);check("coverage real hole",coverage.seconds(.5),.4,1e-9);
    check("coverage unknown length",coverage.seconds(0),-1,0);
    coverage.reset(); for(int i=0;i<1000;++i)coverage.add(i*.001,(i+1)*.001);
    check("coverage random host spans",coverage.seconds(1),1,1e-9);
    coverage.reset();for(int i=0;i<4100;++i)coverage.add(i*2.0,i*2.0+1);
    check("coverage overflow unknown",coverage.seconds(10000),-1,0);

    struct Payload { int id=0, inverse=0; };
    ml::RealtimeAsset<Payload> asset;
    std::atomic<bool> done { false };std::atomic<int> corrupt {0};
    std::thread publisher([&]{for(int i=1;i<=20000;++i){auto p=std::make_unique<Payload>();p->id=i;p->inverse=~i;asset.publish(std::move(p));}done=true;});
    unsigned allocations=0,releases=0;
    while(!done.load())
    {
        rtAudit::Scope audit;
        {const auto read=asset.acquire();if(read && read->inverse!=~read->id)++corrupt;}
        allocations+=rtAudit::allocations;releases+=rtAudit::releases;
    }
    publisher.join();
    check("asset exchange no corrupt",corrupt.load(),0,0);
    check("asset callback new",allocations,0,0);check("asset callback delete",releases,0,0);

    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("ml-runtime", ".wav",false);
    {
        juce::WavAudioFormat format;juce::AudioBuffer<float> source(2,96000);
        for(int i=0;i<96000;++i)for(int ch=0;ch<2;++ch)source.setSample(ch,i,.1f*std::sin(i*.1f));
        auto stream=file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,2,24,{},0));
        writer->writeFromAudioSampleBuffer(source,0,source.getNumSamples());
    }
    {
        ml::ReferencePlayer reference;ml::FilePreviewPlayer preview;preview.prepare(48000);
        reference.load({file.getFullPathName()},48000);
        for(int wait=0;wait<500 && reference.isBusy();++wait)juce::Thread::sleep(2);
        check("reference decoded before ready",reference.isReady(),1,0);
        preview.start(file,.1,.2,true);
        for(int wait=0;wait<500 && !preview.isReady();++wait)juce::Thread::sleep(2);
        check("preview decoded before ready",preview.isReady(),1,0);
        juce::AudioBuffer<float> block(2,512);reference.setEnabled(true);
        unsigned news=0,deletes=0;
        for(int i=0;i<1000;++i)
        {
            block.clear();
            {rtAudit::Scope audit;reference.render(block,.5,true,0);preview.render(block);news+=rtAudit::allocations;deletes+=rtAudit::releases;}
            if(i%50==0){reference.load({file.getFullPathName()},48000);reference.setEnabled(true);preview.start(file,.1,.2,true);}
        }
        check("players callback new",news,0,0);check("players callback delete",deletes,0,0);
        reference.clear();preview.stop();
    }
    file.deleteFile();
    {
        ml::album::Session a,b; a.setAlbumName("project A"); b.setAlbumName("project B");
        check("project instances isolated",a.snapshot().albumName=="project A" && b.snapshot().albumName=="project B",1,0);
        check("project identities unique",a.snapshot().projectId!=b.snapshot().projectId,1,0);
        check("empty project not PASS",!ml::album::Session::evaluate(a.snapshot()).passed(),1,0);
        ml::album::Snapshot delivery;ml::album::TrackRecord track;
        track.label="coverage";track.durationSeconds=10;track.coveragePct=-1;
        track.stats.hasSignal=true;track.stats.integrated=-14;track.stats.truePeakDb=-6;
        delivery.tracks.push_back(track);
        check("unknown coverage not PASS",!ml::album::Session::evaluate(delivery).passed(),1,0);
        delivery.tracks[0].coveragePct=99.999f;
        check("incomplete coverage not PASS",!ml::album::Session::evaluate(delivery).passed(),1,0);
        check("invalid project rejected",!b.restoreState("{}"),1,0);
        a.setReviewed("hash|version|event",true);
        check("review persists in state",a.serializeState().contains("hash|version|event"),1,0);
        const auto state=a.serializeState();
        ml::album::Session restored;check("empty project restored",restored.restoreState(state),1,0);
        check("restored snapshot unchanged",restored.serializeState()==state,1,0);
        check("review restored",restored.snapshot().reviewedEvidence.contains("hash|version|event"),1,0);
        ml::FileForensics before,after;before.sha256=juce::String::repeatedString("a",64);after.sha256=juce::String::repeatedString("b",64);
        ml::FileFinding event;event.code="test";
        check("new source rejects old review",before.evidenceKey(event)!=after.evidenceKey(event),1,0);
    }
}
