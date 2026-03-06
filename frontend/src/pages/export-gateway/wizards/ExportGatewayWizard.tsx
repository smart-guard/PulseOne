// ============================================================================
// wizards/ExportGatewayWizard.tsx  (Refactored - 슬림 조합 버전)
// 원본 백업: ExportGatewayWizard.tsx.bak
// ============================================================================

import React from 'react';
import { Modal, Steps, Button, Space } from 'antd';
import { useWizardState } from './useWizardState';
import { ExportGatewayWizardProps } from './types';

import Step0GatewayInfo from './steps/Step0GatewayInfo';
import Step1Profile from './steps/Step1Profile';
import Step2Template from './steps/Step2Template';
import Step3Target from './steps/Step3Target';
import Step4Schedule from './steps/Step4Schedule';

const ExportGatewayWizard: React.FC<ExportGatewayWizardProps> = (props) => {
    const { visible, onClose } = props;
    const state = useWizardState(props);
    const { confirm, tl } = state;

    return (
        <Modal
            title={<><i className={`fas ${state.editingGateway ? 'fa-edit' : 'fa-magic'} `} style={{ marginRight: '8px' }} />{state.editingGateway ? tl('gwWizard.editTitle', { ns: 'dataExport' }) : tl('gwWizard.createTitle', { ns: 'dataExport' })}</>}
            open={visible}
            onCancel={onClose}
            width={900}
            footer={null}
            maskClosable={false}
            destroyOnClose
            className="wizard-modal"
        >
            <div style={{ display: 'flex', flexDirection: 'column', height: '650px' }}>
                {/* Steps 헤더 */}
                <div style={{ padding: '0 0 24px 0' }}>
                    <Steps current={state.currentStep} size="small" className="site-navigation-steps">
                        {state.steps.map(s => <Steps.Step key={s.title} title={s.title} description={s.description} />)}
                    </Steps>
                </div>

                {/* Step 콘텐츠 */}
                <div className="wizard-content" style={{ flex: 1, overflowY: 'auto', padding: '16px', border: '1px solid #f0f0f0', borderRadius: '12px', background: 'white' }}>
                    {state.currentStep === 0 && (
                        <Step0GatewayInfo
                            tl={tl}
                            isAdmin={state.isAdmin}
                            tenants={state.tenants}
                            sites={state.sites}
                            wizardTenantId={state.wizardTenantId}
                            setWizardTenantId={state.setWizardTenantId}
                            wizardSiteId={state.wizardSiteId}
                            setWizardSiteId={state.setWizardSiteId}
                            fetchSites={state.fetchSites}
                            gatewayData={state.gatewayData}
                            setGatewayData={state.setGatewayData}
                        />
                    )}

                    {state.currentStep === 1 && (
                        <Step1Profile
                            tl={tl}
                            profiles={state.profiles}
                            allPoints={state.allPoints}
                            profileMode={state.profileMode}
                            setProfileMode={state.setProfileMode}
                            selectedProfileId={state.selectedProfileId}
                            setSelectedProfileId={state.setSelectedProfileId}
                            newProfileData={state.newProfileData}
                            setNewProfileData={state.setNewProfileData}
                            mappings={state.mappings}
                            setMappings={state.setMappings}
                            bulkSiteId={state.bulkSiteId}
                            setBulkSiteId={state.setBulkSiteId}
                        />
                    )}

                    {state.currentStep === 2 && (
                        <Step2Template
                            tl={tl}
                            templates={state.templates}
                            templateMode={state.templateMode}
                            setTemplateMode={state.setTemplateMode}
                            selectedTemplateId={state.selectedTemplateId}
                            setSelectedTemplateId={state.setSelectedTemplateId}
                            newTemplateData={state.newTemplateData}
                            setNewTemplateData={state.setNewTemplateData}
                            templateEditMode={state.templateEditMode}
                            setTemplateEditMode={state.setTemplateEditMode}
                        />
                    )}

                    {state.currentStep === 3 && (
                        <Step3Target
                            tl={tl}
                            targets={state.targets}
                            targetMode={state.targetMode}
                            setTargetMode={state.setTargetMode}
                            selectedProtocols={state.selectedProtocols}
                            setSelectedProtocols={state.setSelectedProtocols}
                            targetData={state.targetData}
                            setTargetData={state.setTargetData}
                            selectedTargetIds={state.selectedTargetIds}
                            setSelectedTargetIds={state.setSelectedTargetIds}
                            editingTargets={state.editingTargets}
                            setEditingTargets={state.setEditingTargets}
                            updateTargetPriority={state.updateTargetPriority}
                            totalTargetCount={state.totalTargetCount}
                        />
                    )}

                    {state.currentStep === 4 && (
                        <Step4Schedule
                            tl={tl}
                            transmissionMode={state.transmissionMode}
                            setTransmissionMode={state.setTransmissionMode}
                            scheduleMode={state.scheduleMode}
                            setScheduleMode={state.setScheduleMode}
                            scheduleData={state.scheduleData}
                            setScheduleData={state.setScheduleData}
                            selectedScheduleId={state.selectedScheduleId}
                            setSelectedScheduleId={state.setSelectedScheduleId}
                            availableSchedules={state.availableSchedules}
                        />
                    )}
                </div>

                {/* Footer */}
                <div className="wizard-footer" style={{ padding: '16px 32px', borderTop: '1px solid var(--neutral-100)', display: 'flex', justifyContent: 'space-between', marginTop: 'auto' }}>
                    <div style={{ display: 'flex', gap: '8px' }}>
                        <Button onClick={onClose} disabled={state.loading}>{tl('cancel', { ns: 'common' })}</Button>
                        {state.editingGateway && state.onDelete && (
                            <Button
                                danger
                                onClick={async () => {
                                    const confirmed = await confirm({
                                        title: 'Delete Gateway',
                                        message: `"${state.editingGateway!.name}" 게이트웨이를 정말 삭제하시겠습니까?\n이 작업은 되돌릴 수 없습니다.`,
                                        confirmText: '삭제',
                                        confirmButtonType: 'danger'
                                    });
                                    if (confirmed && state.onDelete) {
                                        await state.onDelete(state.editingGateway!);
                                        onClose();
                                    }
                                }}
                            >
                                삭제
                            </Button>
                        )}
                    </div>
                    <Space>
                        {state.currentStep > 0 && <Button onClick={state.handlePrev} disabled={state.loading}>이전</Button>}
                        {state.currentStep < state.steps.length - 1 ? (
                            <Button type="primary" onClick={state.handleNext}>다음 단계</Button>
                        ) : (
                            <Button type="primary" onClick={state.handleFinish} loading={state.loading}>구성 완료 및 저장</Button>
                        )}
                    </Space>
                </div>
            </div>
        </Modal>
    );
};

export default ExportGatewayWizard;
