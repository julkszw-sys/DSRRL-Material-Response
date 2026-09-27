-- DSRRL telemetry control-plane migration
-- Applied to Supabase on 2026-09-27 as migration: add_effect_activation_telemetry
-- Purpose: normalize effect activation stages independently from resource/receiver telemetry.

create table if not exists dsrrl.addon_effect_telemetry (
  effect_telemetry_id bigint generated always as identity primary key,
  runtime_session_id bigint not null
    references dsrrl.addon_runtime_sessions(runtime_session_id) on delete cascade,
  effect_key text not null
    check (effect_key ~ '^[A-Za-z0-9][A-Za-z0-9._-]*$'),
  receiver_index integer not null default -1
    check (receiver_index >= -1),
  route_index integer not null default -1
    check (route_index >= -1),
  candidate_count bigint not null default 0 check (candidate_count >= 0),
  authority_count bigint not null default 0 check (authority_count >= 0),
  prepared_count bigint not null default 0 check (prepared_count >= 0),
  applied_count bigint not null default 0 check (applied_count >= 0),
  failopen_count bigint not null default 0 check (failopen_count >= 0),
  restore_fail_count bigint not null default 0 check (restore_fail_count >= 0),
  activation_status text not null default 'OPEN'
    check (activation_status = any (array[
      'NOT_SEEN','CANDIDATE_ONLY','AUTHORIZED_NOT_PREPARED',
      'PREPARED_NOT_APPLIED','APPLIED','FAIL_OPEN','PARTIAL','OPEN'
    ]::text[])),
  pixel_status text not null default 'UNVERIFIED',
  failure_stage text,
  failure_code text,
  failure_reason text,
  evidence_id uuid references dsrrl.knowledge_evidence(evidence_id),
  metadata jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique(runtime_session_id,effect_key,receiver_index,route_index)
);

alter table dsrrl.addon_effect_telemetry enable row level security;
revoke all on table dsrrl.addon_effect_telemetry from anon, authenticated;

create index if not exists addon_effect_telemetry_session_idx
  on dsrrl.addon_effect_telemetry(runtime_session_id, activation_status);
create index if not exists addon_effect_telemetry_effect_idx
  on dsrrl.addon_effect_telemetry(effect_key, activation_status);
create index if not exists addon_effect_telemetry_receiver_route_idx
  on dsrrl.addon_effect_telemetry(receiver_index, route_index)
  where receiver_index >= 0 or route_index >= 0;
create index if not exists addon_effect_telemetry_evidence_idx
  on dsrrl.addon_effect_telemetry(evidence_id);

create or replace function dsrrl.upsert_addon_effect_telemetry(
  p_runtime_session_id bigint,
  p_effect_key text,
  p_receiver_index integer default -1,
  p_route_index integer default -1,
  p_candidate_count bigint default 0,
  p_authority_count bigint default 0,
  p_prepared_count bigint default 0,
  p_applied_count bigint default 0,
  p_failopen_count bigint default 0,
  p_restore_fail_count bigint default 0,
  p_activation_status text default 'OPEN',
  p_pixel_status text default 'UNVERIFIED',
  p_failure_stage text default null,
  p_failure_code text default null,
  p_failure_reason text default null,
  p_evidence_id uuid default null,
  p_metadata jsonb default '{}'::jsonb
) returns bigint
language plpgsql
set search_path = dsrrl, pg_temp
as $function$
declare
  v_id bigint;
begin
  if p_runtime_session_id is null then
    raise exception 'runtime_session_id is required';
  end if;
  if p_effect_key is null or p_effect_key !~ '^[A-Za-z0-9][A-Za-z0-9._-]*$' then
    raise exception 'invalid effect_key: %', p_effect_key;
  end if;
  if coalesce(p_receiver_index,-1) < -1 or coalesce(p_route_index,-1) < -1 then
    raise exception 'receiver_index and route_index must be >= -1';
  end if;

  insert into dsrrl.addon_effect_telemetry(
    runtime_session_id,effect_key,receiver_index,route_index,
    candidate_count,authority_count,prepared_count,applied_count,
    failopen_count,restore_fail_count,activation_status,pixel_status,
    failure_stage,failure_code,failure_reason,evidence_id,metadata
  )
  values(
    p_runtime_session_id,p_effect_key,coalesce(p_receiver_index,-1),coalesce(p_route_index,-1),
    coalesce(p_candidate_count,0),coalesce(p_authority_count,0),
    coalesce(p_prepared_count,0),coalesce(p_applied_count,0),
    coalesce(p_failopen_count,0),coalesce(p_restore_fail_count,0),
    coalesce(p_activation_status,'OPEN'),coalesce(p_pixel_status,'UNVERIFIED'),
    p_failure_stage,p_failure_code,p_failure_reason,p_evidence_id,coalesce(p_metadata,'{}'::jsonb)
  )
  on conflict(runtime_session_id,effect_key,receiver_index,route_index)
  do update set
    candidate_count = excluded.candidate_count,
    authority_count = excluded.authority_count,
    prepared_count = excluded.prepared_count,
    applied_count = excluded.applied_count,
    failopen_count = excluded.failopen_count,
    restore_fail_count = excluded.restore_fail_count,
    activation_status = excluded.activation_status,
    pixel_status = excluded.pixel_status,
    failure_stage = excluded.failure_stage,
    failure_code = excluded.failure_code,
    failure_reason = excluded.failure_reason,
    evidence_id = coalesce(excluded.evidence_id, dsrrl.addon_effect_telemetry.evidence_id),
    metadata = dsrrl.addon_effect_telemetry.metadata || excluded.metadata,
    updated_at = now()
  returning effect_telemetry_id into v_id;

  return v_id;
end
$function$;

revoke all on function dsrrl.upsert_addon_effect_telemetry(
  bigint,text,integer,integer,bigint,bigint,bigint,bigint,bigint,bigint,text,text,text,text,text,uuid,jsonb
) from public, anon, authenticated;

create or replace function dsrrl.renderer_addon_effect_telemetry_status(
  p_runtime_session_id bigint default null,
  p_build_key text default null
) returns jsonb
language sql
stable
set search_path = dsrrl, pg_temp
as $function$
with chosen_session as (
  select coalesce(
    p_runtime_session_id,
    (
      select s.runtime_session_id
      from dsrrl.addon_runtime_sessions s
      join dsrrl.addon_builds b on b.addon_build_id=s.addon_build_id
      where p_build_key is null or b.build_key=p_build_key
      order by s.runtime_session_id desc
      limit 1
    )
  ) as runtime_session_id
),
rows as (
  select t.*
  from dsrrl.addon_effect_telemetry t
  join chosen_session c on c.runtime_session_id=t.runtime_session_id
),
status_counts as (
  select activation_status,count(*)::bigint n
  from rows
  group by activation_status
),
effects as (
  select jsonb_agg(
    jsonb_build_object(
      'effect',effect_key,
      'receiver',nullif(receiver_index,-1),
      'route',nullif(route_index,-1),
      'status',activation_status,
      'candidate',candidate_count,
      'authority',authority_count,
      'prepared',prepared_count,
      'applied',applied_count,
      'fail_open',failopen_count,
      'restore_fail',restore_fail_count,
      'pixel',pixel_status,
      'failure_stage',failure_stage,
      'failure_code',failure_code,
      'failure_reason',failure_reason,
      'metadata',metadata
    )
    order by effect_key,receiver_index,route_index
  ) as data
  from rows
)
select jsonb_build_object(
  'runtime_session_id',(select runtime_session_id from chosen_session),
  'rows',(select count(*) from rows),
  'receiver_specific_rows',(select count(*) from rows where receiver_index >= 0),
  'route_specific_rows',(select count(*) from rows where route_index >= 0),
  'by_status',coalesce(
    (select jsonb_object_agg(activation_status,n) from status_counts),
    '{}'::jsonb
  ),
  'effects',coalesce((select data from effects),'[]'::jsonb)
);
$function$;

revoke all on function dsrrl.renderer_addon_effect_telemetry_status(bigint,text)
  from public, anon, authenticated;

create or replace function dsrrl.renderer_addon_control_plane_status(p_build_key text default null)
returns jsonb
language sql
stable
set search_path = dsrrl, pg_temp
as $function$
select jsonb_build_object(
  'current_revision', dsrrl.latest_revision(),
  'build_state', dsrrl.renderer_addon_bootstrap(p_build_key),
  'receiver_registry', dsrrl.renderer_addon_receiver_registry('material_response.c101_stable_hemenv'),
  'effect_telemetry', dsrrl.renderer_addon_effect_telemetry_status(null,p_build_key),
  'infra_counts', jsonb_build_object(
    'builds',(select count(*) from dsrrl.addon_builds),
    'build_features',(select count(*) from dsrrl.addon_build_features),
    'hook_sites',(select count(*) from dsrrl.addon_hook_sites),
    'build_hooks',(select count(*) from dsrrl.addon_build_hooks),
    'runtime_environments',(select count(*) from dsrrl.addon_runtime_environments),
    'runtime_sessions',(select count(*) from dsrrl.addon_runtime_sessions),
    'runtime_events',(select count(*) from dsrrl.addon_runtime_events),
    'runtime_metrics',(select count(*) from dsrrl.addon_runtime_metrics),
    'receiver_telemetry',(select count(*) from dsrrl.addon_receiver_telemetry),
    'effect_telemetry',(select count(*) from dsrrl.addon_effect_telemetry),
    'receiver_registry',(select count(*) from dsrrl.addon_receiver_registry),
    'resource_bindings',(select count(*) from dsrrl.bridge_resource_bindings),
    'crash_signatures',(select count(*) from dsrrl.addon_crash_signatures),
    'diagnostic_experiments',(select count(*) from dsrrl.addon_diagnostic_experiments),
    'compatibility_results',(select count(*) from dsrrl.addon_compatibility_results),
    'checks',(select count(*) from dsrrl.addon_checks)
  ),
  'known_gaps', jsonb_build_array(
    jsonb_build_object(
      'key','resource_material_routing',
      'state',case when (select count(*) from dsrrl.bridge_resource_bindings)=0 then 'EMPTY' else 'POPULATED' end
    ),
    jsonb_build_object(
      'key','receiver_specific_runtime_telemetry',
      'state',case
        when (select count(*) from dsrrl.addon_receiver_telemetry)>0
          or (select count(*) from dsrrl.addon_effect_telemetry where receiver_index>=0)>0
        then 'POPULATED' else 'EMPTY' end
    ),
    jsonb_build_object(
      'key','effect_activation_runtime_telemetry',
      'state',case when (select count(*) from dsrrl.addon_effect_telemetry)=0 then 'EMPTY' else 'POPULATED' end
    ),
    jsonb_build_object(
      'key','diagnostic_experiment_backfill',
      'state',case when (select count(*) from dsrrl.addon_diagnostic_experiments)=0 then 'EMPTY' else 'POPULATED' end
    )
  )
);
$function$;
