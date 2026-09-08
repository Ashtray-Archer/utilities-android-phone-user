# Green Grocer shopper architecture: framework archaeology

> **Status:** research report, not a settled type design.
>
> This report exists to make the next Green Grocer type-design job better informed. Candidate names and boundaries below are hypotheses. Nothing in this report should be read as declaring the final Green Grocer type system, implementation language, UI framework, server framework, persistence model, or deployment architecture.

## 1. Purpose and scope

The concrete specimen is deliberately small:

> **browse → inspect product → add to cart → view cart → change quantity → remove item → continue shopping**

The question is not how to build a whole store. It is narrower:

> **What are the cleanest concepts and boundaries for shopper-facing views and the immediately adjacent application/domain layer?**

This report therefore studies routes/navigation, actions or operations, input parsing and validation, shopper state, cart state, catalog-facing state, presentation data, component/view boundaries, derived values, and the boundary to inventory/fulfillment where grocery semantics force us to notice it.

It intentionally does **not** design payment, checkout implementation, payment providers, seller/admin interfaces, seller actions, inventory-management UI, fulfillment, accounting, persistence/database schema, authentication, or networking/backend deployment. Those are mentioned only when a mature framework has allowed one of those concerns to contaminate the shopper model, or when a clean boundary cannot be stated without naming the neighboring concern.

### Repository baseline

Repository state was checked before writing this report on 2026-09-08.

- Repository owner/name: `Ashtray-Archer/utilities-android-phone-user`
- Repository remote: `https://github.com/Ashtray-Archer/utilities-android-phone-user.git`
- Branch: `green-grocer`
- Head before this report: `a75bbbb26b81c677ed965aea42bd2759f97218a7`
- Green Grocer tree at that head: only `green-grocer/references.md`
- That file is the grocery fixture/reference note and is retained separately from architecture research.
- `green-grocer` and `main` had diverged from merge base `1516f23e4cf975b3f33a10c8237ddbaccb9c9f3f`: the Green Grocer branch had one unique fixture-reference commit while `main` had nine newer unrelated commits at inspection time.

That history is **repository history, not architectural evidence**. The present organization, branch name, directory shape, and any implementation technology elsewhere in this repository are treated as accidental. Nothing below is inferred from Rails, Android, C, Ruby, JavaScript, Elixir, or any other technology merely because it appears in an existing tree or example.

The existing fixture note remains useful and distinct: [references.md](./references.md).

## 2. Research principles

Three labels are used throughout:

- **Observed framework design** — what the framework documentation/source actually does or deliberately proposes.
- **Interpretation** — what architectural problem that design appears to answer, including historical lessons that survive the framework.
- **Provisional Green Grocer implication** — a hypothesis worth testing here, not a final type or architecture decision.

Other rules for this pass:

1. **Framework names are not ontologies.** Rails calling something a model, Phoenix calling something a context, Redux calling something a store, or Spree calling a live cart an order does not make that vocabulary correct for Green Grocer.
2. **Dead projects count.** A framework can lose commercially or disappear while still having isolated an important architectural problem.
3. **Successor frameworks are evidence.** Merb's ideas matter partly because Rails 3 absorbed them. Rack matters partly because it made many Ruby frameworks composable. Pylons/repoze.bfg matter partly because Pyramid made their policy-light ideas more explicit.
4. **The same specimen is more informative than feature lists.** “Add to cart” is forced through different architectures below so that state ownership, input lifetime, rendering, and domain boundaries become visible.
5. **A framework's performance is not its architecture.** Angular can contain useful lifetime/DI/form ideas even if one would never choose Angular for Green Grocer. The same applies to heavyweight Rails or server-held LiveView state.
6. **Stored and conceptually authoritative are different questions.** A production system may cache or denormalize a total. That does not mean `subtotal` is an independent domain fact rather than a derivation from lines and prices.
7. **Parsing is not validation is not availability.** An empty text field, a syntactically valid quantity, and a request the store can currently fulfill are different states.
8. **Presentation strings are not domain values.** `$2.99/lb` is useful shopper-facing text; it should not force money, unit, quantity, and formatting into one opaque value below the presentation boundary.
9. **Current repository shape is not a constraint.** This report may recommend boundaries that imply replacing every current stub.
10. **No final type system in this pass.** The acceptance criterion is a better question set and evidence trail, not premature closure.

## 3. Historical framework map

The frameworks studied do not form one line of “Rails competitors.” Several genuinely different lineages recur.

| Lineage | Representative systems | Architectural problem it emphasizes | Idea that survives |
|---|---|---|---|
| Convention-heavy full stack | Rails, Django, CakePHP, Grails, Play 1, Symfony | Make ordinary CRUD/application work fast by standardizing routes, controllers, templates, persistence, forms, services | Common vocabulary and strong defaults reduce local invention, but defaults can become false domain boundaries |
| Rack/WSGI composition | Rack; Camping, Sinatra, Ramaze, Cuba, Syro, Roda, Scorched; Pylons | Keep HTTP handling as small composable callables/middleware and let the application choose the rest | Transport composition is valuable precisely because it need not own domain architecture |
| Modular consolidation | Merb → Rails 3; Padrino; Hanami | Retain productive conventions without one tightly coupled monolith | Components/actions/views should have explicit APIs and replaceable boundaries |
| Policy-light resource/view | repoze.bfg/Pyramid | Stop pretending web “MVC” vocabulary is universally descriptive | Name the actual web/application roles instead of inheriting desktop MVC mythology |
| Application operation | Trailblazer; service-oriented Grails/Symfony patterns | Keep use-case orchestration outside route/controller objects | “Add to cart” can be an application operation independent of HTTP or renderer |
| Presentation/design first | Pakyow; parts of Nitro; Web Components/Lit later | Make presentation structure explicit and reusable instead of letting backend/persistence objects dictate it | A view can consume semantic presentation data rather than arbitrary internals |
| Minimal Node middleware | Connect, Express, Koa | Provide HTTP composition while deliberately deciding little else | Freedom is real, but every missing decision becomes an application responsibility that must be made consciously |
| Explicit lifecycle/plugin server | hapi | Make request lifecycle, validation, and extension points visible | Dependency/validation boundaries can be explicit without adopting a full application ontology |
| Convention-heavy Node full stack | Sails.js | Supply Rails-like structure on top of Express | Convention can be layered over a minimal transport substrate rather than fused into it |
| Reactive full stack | Meteor | Synchronize server data and browser state reactively | Shared reactive state is powerful but can obscure ownership unless boundaries are explicit |
| Client MV*/reactive | Backbone, Knockout, AngularJS, Ember, modern Angular, Vue, Svelte, Solid | Give browser state and rendering a structure larger than imperative DOM mutation | Local/shared/derived state and state lifetime must be designed; DOM is not the state model |
| Explicit state/action loop | Flux, Redux, Elm | Make changes to state auditable and directional | Semantic actions/messages plus derived projections are useful independent of any specific runtime |
| Server-driven hypermedia | Hotwire/Stimulus, htmx | Keep durable application logic/state on server and send HTML, adding browser behavior only where needed | Not every shopper interaction needs a duplicated browser application state model |
| Stateful server UI process | Phoenix LiveView | Keep interactive UI state in a server process with event → state transition → render | UI state lifetime can be explicit, while domain logic still stays in ordinary application modules |

### Ruby archaeology in more detail

#### Rails and Merb → Rails 3

**Observed framework design.** Rails routes requests to controller actions; actions gather or mutate application data and render/redirect; Action View renders templates. Historically Rails made Active Record-backed models a dominant default vocabulary. In December 2008 the Merb and Rails teams announced their merger: Yehuda Katz wrote that “Merb 2 is Rails 3,” explicitly naming modularity, lower coupling, component opt-in/out, replaceable parts, public APIs, and performance as Merb ideas to carry into Rails 3.

**Interpretation.** Rails demonstrated the enormous productivity gain from a shared application vocabulary, but Merb exposed the cost when every part of that vocabulary is tightly coupled. The important historical lesson is not “use Merb”; it is that a successful convention stack eventually needed explicit component boundaries.

**Provisional Green Grocer implication.** A route/controller/view vocabulary can be useful, but the cart concepts should not become Rails-shaped Active Record objects. If there is an “add to cart” application operation, a renderer or transport should be able to call it without inheriting controller/request objects.

Sources: [Rails Getting Started](https://guides.rubyonrails.org/getting_started.html), [Action View Overview](https://guides.rubyonrails.org/action_view_overview.html), [Yehuda Katz: Rails and Merb Merge](https://yehudakatz.com/2008/12/23/rails-and-merb-merge/), [Rails announcement](https://rubyonrails.org/2008/12/23/merb-gets-merged-into-rails-3), [Rails 3 release notes](https://guides.rubyonrails.org/3_0_release_notes.html).

#### Rack, Camping, Sinatra, Ramaze, Cuba, Syro, Roda, Scorched

**Observed framework design.** Rack reduced the Ruby web boundary to a common callable interface and middleware stack. That enabled many frameworks with radically different opinions to share servers and middleware.

- **Camping** deliberately made a comprehensible tiny framework; its surviving idea is that route/controller/view mechanics need not require a huge hidden runtime.
- **Sinatra** makes a route an HTTP method + pattern + block. It says little about domain modeling.
- **Ramaze** described itself as simple, modular, KISS/POLS, explicitly refusing to force MVC, HMVC, a database toolkit, or a single project structure. Its own history says Rack made Ramaze's earlier ambition to be a “framework for building frameworks” obsolete at the lower HTTP layer; later dispatcher responsibilities were pushed toward Rack middleware.
- **Cuba** calls itself “Ceci n'est pas un framework” and keeps the Rack application small.
- **Syro** follows the Rum/Cuba tradition with an intentionally constrained router, request-local environment/request/response/path state, subapplication composition, and no rendering opinion.
- **Roda** makes routing a tree and deliberately avoids hidden control flow. Its stated goals are simplicity, understandability, performance, extensibility, and reliability; plugins add optional behavior.
- **Scorched** is a lightweight, relatively unopinionated Sinatra descendant exposing Rack, nested controllers, route conditions, and inheritance.

**Interpretation.** These systems are not merely “smaller Rails.” Their central claim is that request dispatch can be explicit enough to read and compose without deciding what a `Product`, `Cart`, or application service is. Roda in particular turns the URL structure into ordinary control flow instead of a global table followed by controller indirection.

**Provisional Green Grocer implication.** Routing and shopper operations should be separable. A route can parse `POST /cart/lines`, locate request/session context, and call `add`, while `add` itself need not know Roda, Rack, cookies, or HTML. This lineage is evidence against putting domain meaning in route/controller class hierarchy.

Sources: [Rack](https://github.com/rack/rack), [Camping](https://github.com/camping/camping), [Sinatra introduction](https://sinatrarb.com/intro.html), [Ramaze project/readme mirror](https://www.ruby-toolbox.com/projects/ramaze), [Ramaze 2009 announcement](https://rubytalk.org/t/ann-ramaze-2009-06/53691), [Cuba](https://cuba.is/), [Syro](https://github.com/soveran/syro), [Roda](https://github.com/jeremyevans/roda), [Scorched](https://scorchedrb.com/).

#### Nitro and Waves

**Observed framework design.** Nitro was a mid-2000s Ruby framework that could be used in MVC or less conventional styles, included its own Og persistence layer, and experimented with XHTML/custom Ruby tags. Waves described routing/matching through “Request Lambdas”; its author emphasized decoupling views from controllers and relying on Rack for the web interface. Historical accounts describe both as attempts to escape limits that early Rails made visible.

**Interpretation.** Their specific APIs did not become dominant, but both are useful reminders that template/controller/ORM coupling was contested very early. The Rails-shaped stack was never the only plausible decomposition.

**Provisional Green Grocer implication.** Do not infer that a “view” must be a template directly fed by ORM objects, or that the same object must simultaneously be a persistence row and shopper concept.

Sources: [InfoQ: Forgotten Ruby Web Frameworks](https://www.infoq.com/news/2007/11/forgotten-ruby-web-frameworks/), [InfoQ: Waves creator interview](https://www.infoq.com/news/2008/02/waves-ruby-framework/), [Ruby Archaeology: Forgotten Web Frameworks](https://www.rubyevents.org/talks/ruby-archaeology-forgotten-web-frameworks).

#### Padrino

**Observed framework design.** Padrino built a fuller application stack on Sinatra while trying to preserve Sinatra's small-core philosophy. It supplied optional helpers, generators, admin facilities, mountable applications, and modular subgems.

**Interpretation.** Full-stack convenience can be layered on a small route/HTTP substrate instead of proving that the full stack is the fundamental architecture.

**Provisional Green Grocer implication.** We can adopt useful conventions for pages/actions without declaring that those conventions own the domain model.

Source: [Padrino framework](https://github.com/padrino/padrino-framework).

#### Hanami / Lotus

**Observed framework design.** Hanami is explicitly composed from single-purpose libraries for router, actions, views, persistence, and other concerns. A Hanami action is a single-purpose object/Rack endpoint with a `#call`/`handle` boundary; dependencies can be injected; actions can be unit tested without a real HTTP server or database. Views are a separate component rather than controller methods with templates attached implicitly.

**Interpretation.** Hanami carries Merb's modularity lesson further: the nearest application boundary can be a small object whose dependencies and output are explicit.

**Provisional Green Grocer implication.** A shopper route should be allowed to depend on a cart application operation and a presentation builder without either becoming the same thing. `add`, `change quantity`, and `remove` are strong candidates for independently testable operations regardless of eventual implementation language.

Sources: [Hanami](https://github.com/hanami/hanami), [hanami-action](https://github.com/hanami/hanami-action), [hanami-router](https://github.com/hanami/router), [hanami-view](https://github.com/hanami/view).

#### Pakyow

**Observed framework design.** Pakyow describes itself as design-first: plain/composable HTML is created first, semantic data bindings are added to describe interface intent, and backend reflection/presentation machinery connects data to that interface.

**Interpretation.** Pakyow is useful here not because Green Grocer should adopt its runtime, but because it reverses the usual direction. The persistence model does not get to dictate the shape of the page merely because it exists first.

**Provisional Green Grocer implication.** Explicit page/view data deserves consideration. A catalog card may need name, shopper-facing price text, quantity affordance, availability message, and navigation target; that does not mean a template should receive an unrestricted `Product`/ORM object.

Sources: [Pakyow](https://github.com/pakyow/pakyow), [Pakyow project description/readme](https://www.ruby-toolbox.com/projects/pakyow).

#### Grape

**Observed framework design.** Grape is an opinionated REST-like API framework for Ruby, centering endpoint routing, parameter declaration/validation, and representations.

**Interpretation.** It is primarily evidence about a transport/interface layer, not a complete shopper application architecture. Its useful separation is that HTTP parameter shape and representation are first-class concerns.

**Provisional Green Grocer implication.** An HTTP/API input quantity is not automatically the domain quantity. Parse/validate the representation before application operations see it.

Source: [Grape](https://github.com/ruby-grape/grape).

#### Trailblazer

**Observed framework design.** Trailblazer explicitly promotes an `Operation` that encapsulates/orchestrates application/business logic outside controllers. Operations return a result and are intended to be callable without HTTP/routing knowledge; optional contracts/forms/representers can occupy neighboring boundaries.

**Interpretation.** This is one of the clearest counterexamples to “controller action = use case.” An HTTP action can be an adapter around an operation.

**Provisional Green Grocer implication.** `add to cart`, `change requested quantity`, and `remove line` should be tested as application operations independent of route and renderer. This does **not** require adopting Trailblazer's class hierarchy or naming.

Sources: [Trailblazer](https://github.com/trailblazer/trailblazer), [Operations documentation](https://trailblazer.to/2.1/docs/operation/).

### Python/PHP/JVM Rails-era comparisons

#### Django

**Observed framework design.** Django's URLconf maps URL patterns to view callables; views accept requests and return responses. Django separately developed substantial forms/validation, template, middleware, and ORM systems.

**Interpretation.** Django demonstrates that even in a strongly integrated framework, URL routing and forms are identifiable boundaries. Its “view” naming also warns against assuming cross-framework terminology matches Rails.

**Provisional Green Grocer implication.** Route resolution, input/form modeling, and presentation can be kept separate even if one eventual framework offers all three.

Sources: [Django URL dispatcher](https://docs.djangoproject.com/en/stable/topics/http/urls/), [Django forms](https://docs.djangoproject.com/en/stable/topics/forms/).

#### Pylons

**Observed framework design.** Pylons assembled WSGI components into a stack and exposed middleware composition rather than insisting on a closed full stack. Controllers were request-facing callables/action dispatchers while template/ORM choices remained relatively open.

**Interpretation.** Pylons is the Python analogue of the “web stack as components” movement around Rack. It matters historically because Pyramid inherited the policy-light side while making its terminology more explicit.

**Provisional Green Grocer implication.** Middleware/transport composition should not be mistaken for application/domain decomposition.

Source: [Pylons documentation](https://docs.pylonsproject.org/projects/pylons-webframework/en/latest/).

#### repoze.bfg / Pyramid

**Observed framework design.** `repoze.bfg` was renamed Pyramid and merged into the Pylons project. Pyramid explicitly says it is not strongly opinionated about ORM, templates, forms, or code arrangement. Its design-defense documentation rejects conventional web MVC terminology as historically misleading: resources and views are sufficient framework concepts, templates are an implementation detail of a view, and the domain model can remain separate from the framework. Pyramid also supports both URL dispatch and traversal.

**Interpretation.** This is especially valuable because it attacks an assumption rather than proposing another mandatory acronym. A web framework's conventional “model/controller/view” labels do not prove that those are the application's clean conceptual boundaries.

**Provisional Green Grocer implication.** Prefer role names that describe Green Grocer: navigation/route, shopper operation, cart/catalog state, and presentation data. Use “controller” or “view model” only when the role actually fits.

Sources: [Pyramid introduction/history](https://docs.pylonsproject.org/projects/pyramid/en/latest/narr/introduction.html), [Defending Pyramid's Design](https://docs.pylonsproject.org/projects/pyramid/en/latest/designdefense.html).

#### TurboGears

**Observed framework design.** TurboGears described itself historically as integrating “best of breed” components, with a path from very small applications to a configured full stack. Its “TurboGears way” material explicitly contrasts Pylons' flexible base with TurboGears' opinionated defaults and admits that some components were healthier as independent packages.

**Interpretation.** An application can gain a coherent vocabulary from integration while preserving replaceable components underneath.

**Provisional Green Grocer implication.** Standard shopper conventions are useful, but we should know which are conveniences versus conceptual necessities.

Sources: [TurboGears](https://turbogears.org/), [The TurboGears Way](https://turbogears.org/welcome/turbogears-way.html).

#### CakePHP

**Observed framework design.** CakePHP is a conventional MVC full stack with controllers/actions, models, routing, helpers/components/behaviors, persistence and validation facilities.

**Interpretation.** It reinforces both the productivity of standard vocabulary and the recurring risk that a framework “model” absorbs persistence, validation, and business rules that are conceptually different.

**Provisional Green Grocer implication.** Do not use “model” as a substitute for deciding whether `Product`, sellable identity, cart line, quantity validation, and availability are actually the same concern.

Source: [CakePHP documentation](https://book.cakephp.org/).

#### Grails

**Observed framework design.** Grails provides controllers, GORM domain classes, Spring-based dependency injection, command objects/validation, and services. Its guidance has long encouraged moving business logic into services instead of bloating controllers; service scope is explicit in the Spring ecosystem.

**Interpretation.** Grails is useful for the same reason modern Angular is useful: scope/lifetime and dependency ownership can be explicit framework concepts instead of invisible globals.

**Provisional Green Grocer implication.** Cart state, catalog lookup, route state, and a single quantity editor may deserve different lifetimes/dependency scopes.

Sources: [Grails services](https://grails.org/), [Grails documentation](https://docs.grails.org/).

#### Play Framework 1.x

**Observed framework design.** Play 1 deliberately described itself as a stateless/share-nothing Java framework synthesizing lessons from Ruby on Rails, Django, Pylons, Symfony, Grails, CakePHP and similar scripting-language stacks, while rejecting much traditional Java Enterprise configuration and slow development cycles.

**Interpretation.** Play 1 is useful as historical cross-framework convergence evidence: by the late 2000s the valuable ideas were already moving across language boundaries. Architecture should therefore be evaluated independently of language identity.

**Provisional Green Grocer implication.** There is no reason to preserve an accidental language merely to preserve a route/action/form idea that exists in many ecosystems.

Source: [Play 1 FAQ](https://www.playframework.com/documentation/1.0/faq).

#### Symfony

**Observed framework design.** Symfony makes routes/controllers, services, a dependency-injection container, forms, and validation separately visible. Controllers can be thin adapters around injected services.

**Interpretation.** Explicit dependency wiring is a strong antidote to global application objects, but a large service container can also hide conceptual ownership if every object can reach everything.

**Provisional Green Grocer implication.** Dependencies should be scoped to the operation/page/component that needs them; “available from container” is not the same as “belongs to this concept.”

Sources: [Symfony routing](https://symfony.com/doc/current/routing.html), [Service container](https://symfony.com/doc/current/service_container.html), [Forms](https://symfony.com/doc/current/forms.html), [Validation](https://symfony.com/doc/current/validation.html).

## 4. Server architecture findings

### 4.1 The route is an interface concern, not the shopping operation

Across Rails, Sinatra/Roda, Django, Express, Hanami, Phoenix, and Pyramid, a route ultimately resolves some external request shape into a callable/action. The important differences are how much hidden dispatch and framework state are attached to that callable.

- Rails: route → controller method.
- Sinatra/Express: route → handler function/block.
- Roda: routing tree → explicit branch/control flow.
- Hanami: route → single-purpose action object.
- Pyramid: URL dispatch or traversal → view callable.
- Phoenix: route → controller action or LiveView.

**Interpretation.** “Route” is best treated as a navigation/transport mapping. “Add to cart” has semantic meaning even if triggered from HTML, JSON, a command-line interface, a test, or another renderer. Conflating the two makes transport changes domain changes.

**Provisional Green Grocer implication.** Keep a boundary of the form:

`external input/navigation → parse/authorize/contextualize → shopper operation → result → presentation`

without yet deciding whether those arrows are objects, functions, messages, or some Idriç/other language construct.

### 4.2 Request state and application state have different lifetimes

Server frameworks mostly agree that request objects are short-lived. They differ on where longer-lived state is kept:

- Rack/Express/Phoenix controllers are ordinarily request-scoped adapters.
- Rails often obtains session/current-user/cart context from request/session or persistence.
- LiveView creates a server process whose UI state survives multiple events until disconnect/termination.
- Node minimal frameworks deliberately refuse to decide where a cart lives.

**Interpretation.** A request is not a cart. A LiveView socket is not a cart. A cookie/session token is not a cart. They are possible holders or locators for state with different lifetimes.

**Provisional Green Grocer implication.** The type-design job should ask separately: what *is* cart state, and what *locates/owns* it for a shopper session? Do not bake HTTP session mechanics into `Cart`.

### 4.3 Forms deserve a boundary of their own

Django forms, Symfony forms/validators, Grails command objects, Phoenix/Ecto changesets, Angular forms, and browser-local edit state all converge on one fact: user input is frequently incomplete or invalid while it is being edited.

A quantity text box can legitimately contain:

- `""`
- `"3"`
- `"3."`
- `"3 lb"`
- an out-of-range value
- a syntactically valid request that current inventory cannot satisfy

Those cannot all be values of one already-valid domain quantity type without either lying or making the domain type absorb editor mechanics.

**Provisional Green Grocer implication.** A temporary quantity-edit/form state is likely distinct from a committed requested quantity. This is a candidate boundary, not yet a settled type name.

### 4.4 Failures should preserve which boundary rejected the input

Mature systems expose different failure layers: parse/shape errors, validation errors, application operation failures, and infrastructure errors.

For Green Grocer, likely examples are:

- `"three-ish"` cannot be parsed as the expected quantity representation;
- `-2 each` is structurally invalid for a committed shopper request;
- `7 lb` may be structurally valid but unavailable now;
- the inventory source may be unreachable.

Treating all four as “invalid quantity” erases useful meaning.

### 4.5 HTTP-independent operations repeatedly reappear

Trailblazer makes this explicit. Hanami action objects make route/request concerns smaller and injectable. Grails/Symfony service guidance pushes business behavior out of controllers. Phoenix contexts are plain Elixir modules behind the web layer.

**Provisional Green Grocer implication.** The shopper operation boundary is one of the strongest findings of this research. The exact operation vocabulary remains open, but route handlers should not be the only place where “add”, “change requested quantity”, or “remove” can exist.

### 4.6 Server lineage comparison against the common questions

The user's 22 comparison questions are normalized here rather than repeated verbatim for every framework.

| Approach | Route / action | Request & shared state | Input / validation / failure | Presentation boundary | HTTP-independent application logic | Scaling/decomposition lesson |
|---|---|---|---|---|---|---|
| Rails | Route resolves controller action method | Request/controller short-lived; session/persistence commonly locate longer state | Params + validations/forms; domain and AR validation can blur | Controller prepares assigns; Action View renders; APIs can render other formats | Possible, but classic apps often let controller + AR model absorb it | Convention scales until controllers/models become “fat”; Merb→Rails 3 added modularity |
| Sinatra | Method/pattern + block | Handler request state; everything else chosen by app | Mostly app/library decision | Renderer/helper chosen by app | Yes, if app creates it | Minimalism makes missing decisions visible but supplies little protection |
| Cuba/Syro/Roda | Explicit request-tree/path matching; route branch is ordinary control flow | Request-local state explicit; shared state application-defined | Application-defined | Rendering optional/separate | Naturally possible | Composition/subapps + visible control flow resist hidden dispatcher complexity |
| Hanami | Route → action object | Action is small/injectable; app dependencies explicit | Params/contracts available around action; result/response explicit | View is separate component | Strongly supported | Single-purpose libraries and DI keep boundaries testable |
| Trailblazer | Controller/route is adapter; operation is use case | Operation receives state/dependencies explicitly | Contract/result patterns possible | Representer/view layer optional | Core design goal | Keep orchestration out of transport; cost is another abstraction vocabulary |
| Django | URL pattern → view callable | Request local; sessions/services/models external | Mature forms + validation | Template/response separate from URL resolver | Possible via ordinary Python services | Full stack gives vocabulary but model/form/view conventions can dominate domain naming |
| Pylons | WSGI/controller action | Request in WSGI stack; app state component-defined | App/component choice | Template stack chosen by app | Yes | WSGI onion shows transport middleware composition clearly |
| Pyramid | URL dispatch/traversal → view callable | Request-local; domain deliberately outside framework | No mandatory built-in form/ORM policy | View returns response; template is implementation detail | Yes | Refusal to force MVC keeps domain terminology open |
| TurboGears | Controller/routing on configured Pylons-style stack | Request local; configured components supply shared services | Integrated defaults | Template/view conventions | Yes with services | Opinionated integration can sit on composable substrate |
| CakePHP | Route → controller action | Request/controller + ORM model state | Model/form validation | Views/helpers | Possible but framework convention often pulls logic into models/controllers | Conventional model can become persistence+validation+domain bucket |
| Grails | Route → controller action | Spring DI services with explicit scopes; GORM domain objects | Command objects/constraints | Views/JSON etc. | Services strongly support it | DI scope is useful; GORM domain can still fuse persistence/domain |
| Play 1 | Stateless route/action | Share-nothing request model; external/shared state explicit | Framework binding/validation | Template/response | Yes | Historical synthesis: productive web ideas are portable across languages |
| Symfony | Route → controller callable | Request plus DI-managed services/scopes | Forms + Validator are explicit systems | Controllers return response/render templates | Strongly supported via services | Explicit wiring scales, but giant containers can make dependency ownership fuzzy |
| Express | Method/path → handler/middleware | Request/response local; shared state intentionally unspecified | App chooses parser/schema/validator | App chooses JSON/templates/etc. | Entirely app's responsibility | “Unopinionated” means architecture debt if team never chooses boundaries |
| Koa | Async middleware onion | `Context` is request-scoped; app services external | App/library choice | App/library choice | Yes | Downstream/upstream middleware makes cross-cutting request behavior explicit |
| hapi | Route handler inside explicit request lifecycle | Request local; plugins/services managed by app/server | Route validation and lifecycle failures are first-class | Handler/response toolkit | Yes | Lifecycle/plugin contracts give more structure than Express without dictating commerce model |
| Sails | Express substrate + actions/blueprints/models | Request plus framework services/ORM | Actions2 inputs/exits provide explicit boundary | Views/API responses | Can be factored into services/actions | Auto-CRUD/convention is fast but can let persistence shape public application API |
| Meteor | Method/publication plus reactive DDP data | Server/client reactive data with Minimongo cache | Method/schema conventions vary | Reactive templates/components | Possible, but shared reactive graph is central | Reactive synchronization is powerful; ownership can become implicit |
| Phoenix controller | Route → controller action; context behind web layer | Request local; context/domain ordinary Elixir | Changesets can parse/cast/validate external input | Render/JSON component separate | Contexts make this natural | Web layer is explicitly an interface to ordinary application modules |
| Phoenix LiveView | Route → stateful LiveView process; events are handlers | Socket assigns live for process; URL params handled separately; reconnect remounts | Forms/events + changesets; focused browser input has its own live editing behavior | `render`/components from assigns | Context should still own domain operation | Very explicit event→state→render loop, but UI process state must not become domain ownership by accident |

## 5. Browser/UI architecture findings

### 5.1 jQuery-era imperative DOM programming: useful baseline precisely because it lacks a state model

**Observed framework design.** jQuery made selection, events, DOM mutation and Ajax portable and convenient. It did not prescribe an application state architecture.

**Interpretation.** In a shopping cart built this way, the displayed number in a DOM node, a hidden input, a JavaScript variable, and a server cart can easily become competing copies of “the quantity.” Imperative mutation does not tell us which is authoritative.

**Provisional Green Grocer implication.** The DOM should not be treated as the domain model merely because it visibly contains the current value.

Source: [jQuery](https://jquery.com/).

### 5.2 Backbone

**Observed framework design.** Backbone separates Models/Collections, Views, Events, and Routers. Models hold data/business-ish behavior and emit change events; Views listen/render/handle UI events; Routers make application states addressable/bookmarkable through URLs/history.

**Interpretation.** Backbone is an early clear statement that URL/navigation state and object data are different concerns. Its weakness for this research is that Backbone Model also leans toward REST persistence, so domain and persistence can still collapse together.

**Provisional Green Grocer implication.** Search/filter/product location belongs naturally in navigation state; cart mutation does not have to be navigation state.

Source: [Backbone](https://backbonejs.org/).

### 5.3 Knockout

**Observed framework design.** Knockout's MVVM approach uses declarative bindings from DOM to a view model and computed observables for values derived from other observables.

**Interpretation.** Computed values are direct precedent for treating a cart subtotal as a projection rather than another independently mutated field.

**Provisional Green Grocer implication.** Prefer one source for lines/quantities/prices and derive item count/subtotal unless later measurement proves caching necessary.

Source: [Knockout](https://knockoutjs.com/).

### 5.4 AngularJS 1.x

AngularJS and modern Angular must not be treated as one architecture.

**Observed framework design.** AngularJS supplied a very broad browser application vocabulary: scopes/controllers, templates/directives, routing, forms, dependency injection, services, and test support. `$scope` formed a hierarchy associated with DOM/controller structure; injectable services were commonly singleton shared-state holders.

**Interpretation.** AngularJS solved the “every team invents its own jQuery application” problem by standardizing vocabulary, but scope inheritance and two-way bindings could make state provenance difficult in large applications.

**Provisional Green Grocer implication.** Standard names for route, form, service and component are useful; implicit hierarchical mutation is not. Cart state should not become reachable/mutable merely because an ancestor scope happens to expose it.

Sources: [AngularJS archive](https://code.angularjs.org/), [AngularJS guide archive](https://docs.angularjs.org/guide).

### 5.5 Modern Angular

**Observed framework design.** Modern Angular is component-based and ships first-party dependency injection, router, forms, testing facilities and signal-based reactivity. Dependency providers can exist at application, route/environment, component/directive and other hierarchical scopes. Reactive/template forms model value, validation and change behavior explicitly; current signal-form work exposes validity/errors/pending as reactive state. Logic can often be tested without depending on final DOM rendering.

**Interpretation.** The most relevant Angular lesson is **not** “Green Grocer should use Angular.” It is that dependency/state lifetime is an architectural dimension worth naming. An application-wide singleton, route-level resource, component-local edit buffer, and derived signal are not interchangeable merely because all are “state.”

A useful Green Grocer decomposition to test is:

- **catalog/query result** — potentially tied to route/search state and cache lifetime;
- **cart state** — shopper-session/application lifetime;
- **route/search state** — URL/navigation lifetime;
- **quantity edit buffer** — component/form lifetime, disposable on cancel/navigation;
- **derived subtotal/count** — no independent mutation lifetime at all.

**Provisional Green Grocer implication.** Explicit lifetimes/scopes are worth carrying forward regardless of framework. It would be a category error to put all of the above in one “global store.” It would also be an error to force all of them local to a component.

Sources: [Angular overview](https://angular.dev/overview), [Dependency injection](https://angular.dev/guide/di), [Hierarchical injectors](https://angular.dev/guide/di/hierarchical-dependency-injection), [Forms](https://angular.dev/guide/forms), [Routing](https://angular.dev/guide/routing), [Testing](https://angular.dev/guide/testing).

### 5.6 Ember

**Observed framework design.** Ember gives routes, models, controllers, templates, services and a strong convention set. Controllers are long-lived/singleton-like and route query params are addressable state.

**Interpretation.** Ember supplies a concrete warning about lifetime mismatch: transient state placed in a long-lived controller can unexpectedly survive route transitions.

**Provisional Green Grocer implication.** Temporary quantity-edit text should not accidentally inherit cart-session lifetime just because both are reachable from the same screen.

Source: [Ember Guides](https://guides.emberjs.com/).

### 5.7 React, Flux, Redux

#### React

**Observed framework design.** React treats component state as private to a component position/identity, encourages lifting shared state to the nearest common owner, and explicitly warns against contradictory, redundant, and duplicated state. React documentation states that each piece of state should have a single source of truth.

**Interpretation.** React's useful contribution here is state ownership, not JSX. It gives a disciplined question: which component/system owns this fact, and can it instead be calculated from other facts?

#### Flux

**Observed framework design.** Flux introduced a unidirectional action → dispatcher → store → view flow and deliberately described itself as different from MVC. Actions describe what occurred; stores own state/logic.

**Interpretation.** Semantic events/actions make mutation inspectable. The weakness is that “store” can become a large undifferentiated global if feature/state lifetimes are not separated.

#### Redux

**Observed framework design.** Redux sharpens this into plain action descriptions, reducer transitions, one-way data flow and selectors. Its current guidance explicitly recommends keeping state minimal and deriving additional values such as filtered data and sums through selectors. It also states that not every state value needs to live in Redux; local component state can coexist.

**Provisional Green Grocer implication.** `cart/lineAdded`, `cart/requestedQuantityChanged`, or similarly semantic intents are plausible UI/application boundary representations, but names are not settled. A subtotal is strongly supported as derived data. Quantity edit text belongs locally until committed; the cart itself may have wider ownership.

Sources: [React: Managing State](https://react.dev/learn/managing-state), [Flux overview](https://facebookarchive.github.io/flux/docs/in-depth-overview/), [Redux fundamentals](https://redux.js.org/tutorials/fundamentals/part-1-overview), [Redux: Deriving Data with Selectors](https://redux.js.org/usage/deriving-data-selectors).

### 5.8 Vue

**Observed framework design.** Vue describes a component as combining state, view and actions, then introduces shared state management when multiple components require coordinated state. Computed/reactive values separate derivations from mutable sources.

**Interpretation.** Vue reinforces the rule that “shared” is a consequence of ownership needs, not a default status for all application data.

**Provisional Green Grocer implication.** A product page's open/closed detail UI and quantity editor can stay local while cart state is shared across browse/product/cart views.

Source: [Vue state management](https://vuejs.org/guide/scaling-up/state-management.html).

### 5.9 Svelte

**Observed framework design.** Svelte/SvelteKit provide local reactive state, derived state, stores where broader sharing is needed, and server form actions/progressive enhancement in SvelteKit.

**Interpretation.** Svelte demonstrates that an implementation can have rich reactive ergonomics without requiring a centralized global application store. SvelteKit form actions also blur the simplistic “SPA versus server app” divide.

**Provisional Green Grocer implication.** The architecture should survive either local reactive rendering or server form submission; therefore cart semantics should not be defined as a particular browser store mechanism.

Sources: [Svelte documentation](https://svelte.dev/docs), [SvelteKit form actions](https://svelte.dev/docs/kit/form-actions).

### 5.10 Elm

**Observed framework design.** The Elm Architecture is deliberately explicit: `Model` contains state, `View` maps state to UI, messages represent events, and `update` maps message + model to a new model (plus commands/effects in larger programs). Redux openly acknowledges this lineage.

**Interpretation.** Elm is the cleanest pressure test for whether Green Grocer can say what the state actually is. If the only way to represent the shopper interaction is a tangle of hidden object mutation, the concepts are probably not yet clean.

**Provisional Green Grocer implication.** It should be possible to describe cart operations as explicit state transitions even if the final implementation is not functional or Elm-like. The application can still decompose by feature so that “one Model” does not become “one giant record.”

Source: [Elm Architecture](https://guide.elm-lang.org/architecture/).

### 5.11 Solid

**Observed framework design.** Solid uses fine-grained signals: reactive consumers subscribe to specific values rather than requiring whole component re-execution. Its state guidance still separates state/view/actions conceptually.

**Interpretation.** Rendering granularity and state ownership are separate questions. Efficient fine-grained updates do not tell us what the domain boundary is.

**Provisional Green Grocer implication.** Do not design cart types around a renderer's invalidation model.

Source: [Solid signals](https://docs.solidjs.com/concepts/signals).

### 5.12 Stimulus / Hotwire

**Observed framework design.** Stimulus intentionally does not render HTML; it enriches existing HTML with small controllers, actions and targets. Turbo makes navigation/forms server-driven and exchanges HTML rather than requiring a client-side JSON application for ordinary interactions.

**Interpretation.** The Hotwire lineage is a direct challenge to the premise that shopper state must be duplicated into a large browser model. Durable cart state can remain on the application/server side while small browser-only state stays local.

**Provisional Green Grocer implication.** The type/application design should not assume a SPA. If `add`, `change`, and `remove` are transport-independent operations, they can support Turbo/ordinary forms as easily as JSON/browser state.

Sources: [Stimulus](https://stimulus.hotwired.dev/), [Turbo handbook](https://turbo.hotwired.dev/handbook/introduction).

### 5.13 htmx

**Observed framework design.** htmx treats HTML/hypermedia as the state-transfer interface and argues that complex client state should be reserved for genuinely front-end-local concerns. Server responses carry the next hypermedia representation and available actions.

**Interpretation.** htmx is useful because it makes a state ownership distinction visible: system-of-record shopping state can stay server-side while an open menu, focused input, or transient editor state is local.

**Provisional Green Grocer implication.** Cart state and quantity edit buffer should not be collapsed merely because one client-heavy framework could put both in JavaScript.

Source: [htmx: Hypermedia-Friendly Scripting](https://htmx.org/essays/hypermedia-friendly-scripting/).

### 5.14 Web Components / Lit

**Observed framework design.** Web Components standardize custom elements, Shadow DOM and template/encapsulation primitives. Lit adds declarative rendering and distinguishes public reactive properties (component inputs/API) from internal reactive state.

**Interpretation.** This is a useful local presentation boundary, not a whole-application architecture.

**Provisional Green Grocer implication.** A `cart-line` component could receive explicit presentation values and emit intents without owning the canonical cart domain object. Component encapsulation does not answer where cart state belongs.

Sources: [MDN Web Components](https://developer.mozilla.org/en-US/docs/Web/API/Web_components), [Lit reactive properties/state](https://lit.dev/docs/components/properties/).

### 5.15 Browser architecture comparison

| Approach | UI state ownership/lifetime | Actions/events | Derived values | Forms/validation | Navigation | Main growth failure to avoid |
|---|---|---|---|---|---|---|
| jQuery | Implicit in DOM/variables/server | Event callbacks | Manual | Manual | Manual/browser links | Duplicate invisible sources of truth |
| Backbone | Model/Collection; View-local behavior | Events and model mutations | Manual/computed by app | App-defined | Router/history explicit | Model can become persistence+domain+UI bucket |
| Knockout | View-model observables | Bound handlers | Computed observables | Bindings/plugins/app | Separate | Large view models can become catch-alls |
| AngularJS | Scope hierarchy + singleton services | Directive/controller handlers | Watches/expressions | Strong forms | Router | Scope inheritance and two-way mutation obscure provenance |
| Modern Angular | Component signals/forms + DI-scoped shared services | Methods/signals/events | Computed signals | Strong typed/reactive form concepts | First-party router; route providers/resolvers | Over-centralizing services or over-frameworking domain |
| Ember | Route/controller/service hierarchy | Actions | Computed/derived | Framework conventions | Strong URL/query-param model | Long-lived controller/service state can outlive intended UI |
| React | Component owner; lift to closest common owner | Event handlers/reducers | Calculate; avoid redundant state | App/library | Router library; URL often state | Prop/global-store sprawl if ownership not designed |
| Flux/Redux | Explicit store(s); component-local state still possible | Semantic actions; reducers | Selectors | App/library | Router integration | Giant global store and action boilerplate |
| Vue | Component-local by default; shared store as needed | Methods/actions | Computed | Framework/forms ecosystem | Vue Router | Shared store used as default instead of necessity |
| Svelte | Local `$state`/stores; server form state in SvelteKit possible | Handlers/actions | `$derived`/derived stores | Native/SvelteKit form actions | SvelteKit/router | Letting reactive convenience define domain ownership |
| Elm | Explicit Model | Typed Msg → update | Pure functions | Explicit model/messages | URL can be modeled explicitly | One giant model/update if not modularized |
| Solid | Fine-grained signals | Handlers/actions | Memos/derived | App/router ecosystem | Router library | Confusing reactive granularity with domain decomposition |
| Hotwire/Stimulus | Durable state largely server-side; local DOM/controller state | Forms/links + small controllers | Mostly server; local as needed | Server forms + HTML errors | URL/Turbo visits | Over-coupling domain operations to HTML response shape |
| htmx | Durable state server-side; ephemeral browser state local | Hypermedia actions/HTTP attributes | Mostly server | HTML forms/server validation | Hypermedia/URL | Making every interaction a request when genuinely local state is better |
| Web Components/Lit | Component public properties + private local state | DOM/custom events | Reactive updates | Component/app-defined | Separate | Mistaking component encapsulation for application architecture |

## 6. Node / JavaScript server lineage

### 6.1 Connect and Express: deliberate non-decisions

**Observed framework design.** Connect is a sequential middleware stack over Node HTTP. Express adds routing and common web conveniences while describing itself as fast, unopinionated and minimalist.

Express deliberately does **not** decide:

- what a product/cart domain model is;
- whether cart state is in memory, session, database or elsewhere;
- whether application operations are route handlers, services or functions;
- how forms/validation are modeled;
- whether templates receive persistence/domain objects or page data;
- how dependency lifetimes/scopes are managed;
- whether totals are stored or derived;
- how a large app decomposes beyond routers/middleware/modules.

**Interpretation.** This freedom is genuine. The consequence is also genuine: if Green Grocer used an Express-like substrate, every one of those decisions would still have to be made. “Express does not require architecture X” is not evidence that architecture does not matter.

**Provisional Green Grocer implication.** A transport should be replaceable enough that Express could front the same application operations without becoming their definition.

Sources: [Express](https://expressjs.com/), [Connect](https://github.com/senchalabs/connect).

### 6.2 Koa

**Observed framework design.** Koa, from the Express lineage, emphasizes async middleware composition with downstream/upstream “onion” flow and a request-scoped `Context`; it deliberately bundles little middleware.

**Interpretation.** Request lifecycle composition can be elegant while still leaving application state ownership open.

**Provisional implication.** Authentication/logging/error conversion later can wrap shopper operations without entering cart types.

Source: [Koa](https://koajs.com/).

### 6.3 hapi

**Observed framework design.** hapi makes the request lifecycle unusually explicit: route lookup, authentication, validation, handler execution and extension points are named phases. Routes can declare validation and handlers receive request/toolkit objects.

**Interpretation.** Compared with Express, hapi demonstrates the value of making interface validation and lifecycle dependencies explicit without declaring the business/domain ontology.

**Provisional implication.** Green Grocer should preserve which layer rejected a quantity rather than collapsing route-shape validation, domain quantity validation and availability failures.

Source: [hapi](https://hapi.dev/).

### 6.4 Sails.js

**Observed framework design.** Sails builds a Rails-like convention stack on Express, including actions, models/ORM and “blueprint” routes. Actions2 adds typed inputs/exits that can make an action less directly coupled to raw request/response details.

**Interpretation.** Sails is evidence that convention-heavy structure is a layer one can add atop minimal middleware. Blueprint CRUD also demonstrates the risk of letting persistence automatically define the public application interface.

**Provisional implication.** “A cart line exists in storage” must not automatically generate the right shopper operation semantics.

Source: [Sails](https://sailsjs.com/).

### 6.5 Meteor

**Observed framework design.** Meteor's distinctive architecture synchronizes server publications with reactive client-side data (historically Minimongo over DDP), causing UI computations to rerun when dependencies change.

**Interpretation.** Meteor attacks latency/synchronization rather than route/controller decomposition. It demonstrates a very different answer: treat some server/client data as one reactive graph.

**Provisional implication.** Even if Green Grocer later uses synchronization/reactivity, cart ownership and domain transitions should remain explicit enough to resolve concurrent or failed changes; “reactive data arrived” is not itself the semantic operation.

Source: [Meteor documentation](https://docs.meteor.com/).

## 7. Phoenix / LiveView findings

Phoenix is not treated as “Rails in Elixir.” Its BEAM process model and LiveView state lifecycle deserve separate attention.

### Phoenix controllers and contexts

**Observed framework design.** Phoenix routes to controllers/actions much like other server frameworks, but Phoenix project guidance places the web layer as an interface to an Elixir application. Contexts are ordinary modules grouping a coherent API around a domain area. They need not know about HTTP.

**Interpretation.** A context is useful here mainly as evidence for an application boundary: the web renderer should not own cart semantics.

### Ecto changesets

**Observed framework design.** Ecto changesets can cast external parameters, validate data, represent errors, and can even be used schemalessly rather than requiring a database row.

**Interpretation.** This is strong evidence for separating an external edit/form representation from a validated domain value. Changeset is not itself the Green Grocer type answer, but it shows that “form validation” need not be fused to persistence.

### LiveView

**Observed framework design.** A LiveView normally has one server process holding UI state in socket assigns. The lifecycle is roughly initial HTTP/mount → events → `handle_event` state changes → render diffs. URL/navigation parameters can be handled through `handle_params`. A reconnect creates/remounts process state rather than making it permanent domain state. LiveView forms send change/submit events; the browser can temporarily remain the source of truth for the currently focused input while the server supplies validation/render state.

**Interpretation.** LiveView makes state lifetime unusually tangible:

- cart/domain state may outlive a LiveView process;
- socket assigns are UI process state;
- URL params are navigation state;
- focused input text has browser/form editing behavior;
- domain operations should live behind contexts/ordinary modules.

Also, LiveView's `temporary_assigns` optimization must not be confused with the semantic idea of temporary quantity-edit state; they solve different problems.

**Provisional Green Grocer implication.** The architecture should permit a server-held interactive UI without equating UI-process lifetime with cart lifetime. The same conceptual split also benefits client-rendered UIs.

Sources: [Phoenix controllers](https://hexdocs.pm/phoenix/controllers.html), [Phoenix contexts](https://hexdocs.pm/phoenix/contexts.html), [Ecto.Changeset](https://hexdocs.pm/ecto/Ecto.Changeset.html), [Phoenix LiveView](https://hexdocs.pm/phoenix_live_view/Phoenix.LiveView.html), [LiveView forms](https://hexdocs.pm/phoenix_live_view/form-bindings.html).

## 8. Commerce-system findings: Spree and Solidus

Generic frameworks do not tell us what a sellable thing or cart line is. Mature commerce systems therefore need a separate pass.

### 8.1 Spree: Product is not the purchasable unit

**Observed framework design.** Current Spree documentation distinguishes `Product` from `Variant`. Product is the descriptive parent; a Variant is the actual purchasable unit and owns SKU, barcode, price and inventory-related data. A product has a “master variant” convention as well as option variants. Variant pricing can vary by currency/market through Price records.

**Interpretation.** This is strong evidence against one overloaded `Product` object representing both “what shoppers understand this thing to be” and “the exact seller-specific purchasable proposition.” It is weaker evidence for the name `Variant`, because a store may have a sellable item with no shopper-visible variation.

**Provisional Green Grocer implication.** Preserve a possible distinction between descriptive product/catalog identity and seller-specific sellable identity. Test whether `offer`, `sellable`, or something else communicates this better than `variant`.

Sources: [Spree Products](https://spreecommerce.org/docs/developer/core-concepts/products), [Spree Pricing](https://spreecommerce.org/docs/developer/core-concepts/pricing).

### 8.2 Spree: live cart and later transaction share `Order`

**Observed framework design.** Spree's Order documentation explicitly says a cart is “simply an order in cart state”; that same model proceeds through checkout and completion. Line items link a Variant and quantity, and line-item price is locked rather than automatically changing when the Variant's later price changes. Storefront cart API responses expose checkout/payment/shipment/address concerns alongside cart data.

**Interpretation.** This is exactly the architecture Green Grocer should not inherit automatically. It is convenient to reuse one lifecycle object, but it makes the live shopper cart structurally adjacent to checkout/payment/shipment concerns before they are needed. The same object can become the path by which unrelated lifecycle responsibilities accumulate.

**Provisional Green Grocer implication.** `cart ≠ completed order/transaction` remains a serious hypothesis, strengthened rather than weakened by seeing the costs of Spree's choice. We still need the next type-design pass to decide whether there is any useful shared abstraction between them.

Sources: [Spree Orders](https://spreecommerce.org/docs/developer/core-concepts/orders), [Spree create cart API](https://spreecommerce.org/docs/api-reference/storefront/carts/create-a-cart), [Spree line-item quantity API](https://spreecommerce.org/docs/api-reference/storefront/cart-line-items/set-line-item-quantity), [Spree remove line-item API](https://spreecommerce.org/docs/api-reference/storefront/cart-line-items/remove-a-line-item).

### 8.3 Solidus: the same lineage makes the accumulation visible in source

**Observed framework design.** Solidus inherits the Spree vocabulary. Its `Spree::Product` delegates SKU, price, GTIN and related sellable concerns to the master `Variant`; `Variant` owns SKU, prices, option values and inventory-related methods. Its `Order` source explicitly describes the object as the customer's cart until completion and then the permanent transaction record. The class also participates in state machines, payment/shipping/returns, line items, inventory validation and many mutation/business responsibilities.

**Interpretation.** Solidus `Order` is concrete evidence for the “lifecycle god object” risk. A decision that initially sounds economical—cart and order are the same object in different states—creates a natural attachment point for every later checkout concern.

**Provisional Green Grocer implication.** Keep the shopper cart boundary small until evidence requires it to absorb later lifecycle concepts. Do not name a live cart `Order` merely because mature Rails commerce systems do.

Sources: [Solidus Product source](https://github.com/solidusio/solidus/blob/main/core/app/models/spree/product.rb), [Solidus Variant source](https://github.com/solidusio/solidus/blob/main/core/app/models/spree/variant.rb), [Solidus Order source](https://github.com/solidusio/solidus/blob/main/core/app/models/spree/order.rb), [Solidus repository](https://github.com/solidusio/solidus).

### 8.4 Commerce lessons that are stronger than framework vocabulary

The mature systems provide useful evidence for these distinctions without forcing their exact names:

1. **Descriptive catalog grouping and sellable identity differ.** Spree/Solidus Product versus Variant is repeated mature evidence.
2. **Line item and sellable thing differ.** A line records the shopper/cart-specific requested quantity and pricing context for a sellable identity.
3. **Seller-local and global trade identity differ.** SKU and GTIN/barcode are not synonyms.
4. **Price at a line/transaction boundary can differ from current catalog price.** Spree explicitly locks line-item price against later Variant price changes.
5. **Inventory availability is neighboring information, not the definition of quantity.** Solidus asks whether a Variant can supply a requested count; it does not make integer parsing itself an inventory operation.
6. **The cart/order conflation is a choice, not a law.** Its later complexity is visible.

Fixture research is deliberately not mixed into this section. The previously collected grocery/mock repositories remain in [references.md](./references.md).

## 9. Shopping-cart specimen comparisons

This section forces architecturally distinct systems through the exact same Green Grocer interaction. These are sketches of control/state boundaries, not recommendations to implement any framework.

### 9.1 Rails-style

**Browse/product.** `GET /products` and `GET /products/:id` route to controller methods. Controller queries catalog data and renders templates.

**Add.** `POST /cart/lines` receives product/variant identity and quantity params. In a conventional small Rails app the controller may call an Active Record cart/order model directly; a healthier variant calls an application service/operation.

**Cart/change/remove.** `GET /cart`; `PATCH /cart/lines/:id`; `DELETE /cart/lines/:id`.

**State ownership.** Request state belongs to controller/request. Cart is located via session/account/persistence. Temporary input lives in params/form fields. Canonical cart should not be controller instance state.

**Totals.** Can be derived from lines for presentation; production Rails commerce systems may store/cache recalculated totals for persistence/performance, which is a separate concern.

**Invalid quantity.** Parse/form/model validation error returned to view. Availability may be a later operation failure.

**Presentation boundary.** Classic Rails often gives templates domain/AR objects directly; this is convenient but leaks persistence/domain details. A page-data/presenter boundary would make renderer swaps easier.

**What changes with another renderer/transport?** If logic lives in controllers/Active Record callbacks, much changes. If controller is an adapter around cart operations, only input/output adapters change.

### 9.2 Roda/Cuba/Syro routing-tree style

**Browse/product.** The request path is consumed through an explicit routing tree. `/products` branches to list; `/:id` branches to product detail.

**Add/change/remove.** Route branches parse method/path/body then call plain cart operations.

**State ownership.** Request-local route state is explicit. Cart lookup/ownership is application-defined; the router does not pretend to solve it.

**Totals/validation.** Application-defined; route can convert operation results to HTML/JSON.

**Presentation boundary.** No required renderer. The app can create page data and pass it to template/JSON/component renderer.

**Renderer/transport change.** Route tree changes; cart operation need not.

**Lesson.** Extremely little framework machinery is needed to express the specimen. That is evidence that the domain should not depend on a large framework ontology.

### 9.3 Hanami-style

**Route.** Each external route resolves a small action object.

**Add.** `Cart::AddAction` receives parsed request, injects a cart operation/catalog lookup as dependencies, invokes them, and chooses response status/navigation.

**Change/remove.** Separate single-purpose actions rather than a giant CartController.

**State ownership.** Action/request is short-lived; cart dependency has a different lifetime; views consume explicit data.

**Invalid quantity.** Input contract/form boundary can reject shape; operation result can represent domain/application failure.

**Presentation.** Separate Hanami-style view makes page-data boundary natural.

**Renderer/transport change.** Application operations survive; action/view adapters change.

### 9.4 Trailblazer-style operation

Think in use cases first:

- `BrowseCatalog.call(query: ...)`
- `InspectProduct.call(identity: ...)`
- `AddToCart.call(cart:, sellable:, requested_quantity: ...)`
- `ChangeCartQuantity.call(cart:, line:, requested_quantity: ...)`
- `RemoveCartLine.call(cart:, line: ...)`

The names are illustrative only.

**State ownership.** Operations receive the state/dependencies they require rather than reading global HTTP state.

**Input.** A contract/form can parse and validate external representation before operation execution.

**Failure.** Operation result can distinguish invalid request, unavailable sellable, unavailable requested amount, etc.

**Presentation.** Presenter/representer/view turns result into HTML/JSON/UI data.

**Renderer/transport change.** Minimal effect on operation.

**Lesson.** This is the cleanest server-side demonstration that “action” need not mean “HTTP method in controller.”

### 9.5 Express-style minimal routes

**Route.** `app.get('/products', ...)`, `app.post('/cart/lines', ...)`, etc.

**State ownership.** Express does not decide. The team must decide whether cart state is session/application/repository/service state and whether quantity editor state exists only in browser/form.

**Add.** The handler can either contain all logic—a quick path to transport/domain coupling—or call `cartService.add(...)`/equivalent.

**Validation.** Must be chosen: schema validator, form parser, operation validation, or ad hoc `if` statements.

**Presentation.** JSON/template/redirect is entirely application choice.

**Renderer/transport change.** Good if route is thin; painful if route handler became the use case.

**Lesson.** Express proves that framework minimalism does not eliminate architectural questions; it hands them back to us.

### 9.6 Modern Angular

**Browse/product.** Router controls navigable catalog/product state. Route parameters/search query can be the durable/shareable representation of catalog navigation. A catalog service/resource may live at route/feature/application scope depending on caching needs.

**Cart.** A cart service/store can have shopper-session/application lifetime, independent of the currently mounted product component.

**Add.** Product component emits/calls a semantic cart operation through injected service rather than mutating display widgets directly.

**Quantity edit.** A form control/signal belongs to the cart-line editor component. It may temporarily be empty/invalid. Commit sends a valid parsed requested quantity to cart operation.

**Totals.** Computed signal/selectors from cart lines; not independently set by every add/remove handler.

**Invalid quantity.** Form errors are component/form state. Availability rejection from application operation is separate and can be displayed alongside it.

**Navigation.** Router state is not the cart. Returning “continue shopping” restores/navigates catalog/product route without having to copy catalog navigation into cart state.

**Presentation boundary.** Components receive display-ready values or selectors/resources. Domain operations can be tested separately from DOM.

**Renderer/transport change.** Angular component/router/form layer changes; pure application/domain operations can survive if not written as Angular services full of browser concerns.

**Lesson.** Explicit scope/lifetime is the valuable idea; Angular itself is not thereby selected.

### 9.7 React + Redux-style explicit state/actions

One possible state partition (not a final model):

- route/search state: URL/router;
- cart canonical state: Redux or another shared application owner;
- catalog cache: data-fetch layer or feature state;
- line edit text: local component state;
- subtotal/count: selectors.

**Add.** Component dispatches a semantic action/command such as “add requested sellable quantity.” Side-effect/application layer calls backend/operation if necessary; reducer applies successful cart state.

**Change.** Typing changes local editor state. Commit dispatches a parsed quantity change. This prevents an empty input from becoming an impossible cart quantity.

**Remove.** Explicit remove action, not a side effect of DOM deletion.

**Invalid quantity.** Local parse/constraint error versus operation/availability failure remain distinguishable.

**Navigation.** Router/URL state independent of cart; “continue shopping” is navigation, not cart mutation.

**Totals.** Selector derivation is the strongest Redux lesson for Green Grocer.

**Renderer/transport change.** React components change. Reducer/domain operations can survive if framework-independent; network effect layer changes with transport.

### 9.8 Elm

A deliberately explicit sketch:

- `Model` contains catalog/navigation state, canonical cart state, and any active edit buffers as distinct fields/submodels.
- `Msg` includes browse/navigation messages, add intent, edit-text changes, commit quantity change, remove intent, and operation responses.
- `update` is the only state transition entry point.
- `view` derives page structure and totals from model.
- commands perform transport effects.

**Add/change/remove.** Each is an explicit message; domain/cart transition can be a pure module function called from `update`.

**Invalid quantity.** Edit model can represent invalid text without corrupting cart model. Commit only occurs after parse/validation.

**Totals.** Pure derivation.

**Navigation.** URL can be represented explicitly and decoded into page state.

**Renderer/transport change.** Elm's view runtime is obviously specific, but the state-transition/domain module can be independent.

**Lesson.** If Green Grocer cannot write a coherent state-transition story this explicit, the concepts are probably still blurred. We need not adopt Elm to use that test.

### 9.9 Phoenix LiveView

**Browse/product.** URL routes to a LiveView; route/search params can be handled as URL state. Catalog data loaded through a context.

**Cart.** Domain/application cart lives behind a context or ordinary module; socket assigns hold the current UI projection and process-local state, not necessarily the canonical lifetime of the cart.

**Add.** `phx-click`/form event → `handle_event` → parse/change → context operation → refresh assigns → render diff.

**Quantity edit.** Form data/changeset can carry incomplete/invalid editing state. Server validates as events arrive; focused browser input still has local editing behavior.

**Totals.** Derived in context/presentation from cart lines or calculated for assigns, not independently mutated by component.

**Invalid quantity.** Changeset/form error for shape/domain validation; availability operation failure separately represented.

**Navigation.** Live navigation/URL params independent of cart mutation.

**Renderer/transport change.** LiveView event/render code changes. Context/domain functions should survive and can also serve controller/JSON/CLI callers.

**Lesson.** Explicit UI-process lifetime is valuable. Do not make socket lifetime the cart type.

### 9.10 htmx / Hotwire-style server-driven UI

**Browse/product.** Ordinary GET links/routes return HTML/full or partial pages.

**Add.** Form/button POSTs sellable identity + quantity. Server route calls cart operation and responds with updated cart badge/line/page HTML or redirect.

**Cart/change/remove.** Quantity form PATCH/POST and remove DELETE/POST; Turbo/htmx can replace only the line/totals region while canonical cart remains server/application state.

**Quantity edit.** Raw text lives in input DOM until submission/change event; it need not enter canonical cart state on every keystroke.

**Invalid quantity.** Server returns form/fragment with errors while retaining raw edit value for correction.

**Totals.** Derived server-side into page data/HTML.

**Navigation.** URL and links remain first-class; “continue shopping” is an ordinary navigation target.

**Presentation boundary.** HTML is much closer to transport in this style, but the cart operation can remain independent.

**Renderer/transport change.** HTML adapter changes; domain/cart operations do not. Compared with SPA architectures, less application state is duplicated into browser.

### 9.11 What the specimen reveals across all ten

The common interaction can be expressed cleanly in every architecture **without** requiring:

- `Cart` to know about HTTP routes;
- `Product` to be a database row;
- a quantity text field to be the canonical quantity;
- subtotal to be independently mutable state;
- a live cart to be called `Order`;
- a single global UI store;
- a SPA;
- server-driven HTML;
- one implementation language.

The architectural disagreement is mostly about **where state lives and how transitions are mediated**, not about whether the shopper concept “add a sellable thing with a requested amount to a cart” can exist independently of the renderer.

## 10. Grocery-specific pressure tests

Generic ecommerce examples often assume integer quantity and packaged products. Grocery breaks that assumption quickly.

### 10.1 Each versus weight

**Observed mature systems.** Square's order line-item API permits quantity as either count or measurement; with a quantity unit it can represent fractional amounts such as pounds. Instacart's current product-quantity documentation explicitly distinguishes per-unit, by-weight, and variable-weight products. For order requests it uses count and/or weight according to the product quantity type.

**Interpretation.** A raw `quantity : number` loses at least:

- whether the number counts discrete units or measures a continuous quantity;
- the unit (`each`, `lb`, `kg`, `oz`, etc.);
- valid precision/step;
- which measure the seller uses for pricing.

A “3” attached to canned tomatoes and a “3” attached to apples are not the same request merely because both serialize as numbers.

**Provisional Green Grocer implication.** The next type-design pass should test a unit-aware requested amount/quantity representation. Do not yet assume the correct shape is a sum such as `Count | Weight`; a dimension/unit-aware generic quantity or an offer-specific quantity policy may fit better. The important conclusion from evidence is narrower: **unit semantics must not be discarded into an unlabelled number.**

Sources: [Square OrderLineItem](https://developer.squareup.com/reference/square/objects/OrderLineItem), [Instacart product quantity types](https://docs.instacart.com/connect/fulfillment_guide/concepts/product_quantity).

### 10.2 Requested versus measured/fulfilled quantity

**Observed mature system.** Instacart's current fulfillment API exposes separate `qty_requested` and `qty_fulfilled` fields, with units. Its order-item documentation describes requested quantity as what the customer originally ordered and fulfilled quantity as the actual quantity picked/delivered. The product quantity guide separately recognizes variable-weight products and `par_weight` behavior.

**Interpretation.** This directly validates the grocery distinction the provisional Green Grocer discussion raised. A shopper can request an amount before the exact fulfillment measurement exists. The later measurement is not merely an updated copy of the same fact; it answers a different question at a later lifecycle stage.

**Provisional Green Grocer implication.** The shopper cart should be able to express the **requested** amount without designing fulfillment. The eventual measured/fulfilled amount belongs across a neighboring boundary that this report names but does not design. Avoid making shopper quantity representation depend on a future picker/scale result.

Source: [Instacart create delivery order / OrderItem fields](https://docs.instacart.com/connect/api/fulfillment/delivery/create_order/).

### 10.3 SKU versus UPC/EAN/GTIN/barcode

**Observed mature systems.** Spree/Solidus variants carry both SKU and barcode/GTIN-like fields. GS1 defines GTIN as a globally standardized trade-item identifier; UPC and EAN forms are GTIN encodings/number structures, while a barcode is the data carrier. Instacart transaction files separately expose retailer catalog reference code and barcode/PLU.

**Interpretation.** The authorities differ:

- **SKU** — seller/retailer-local identity under that seller's namespace and business rules;
- **GTIN** — standardized trade-item identity assigned in the GS1 system;
- **barcode** — a machine-readable carrier which may encode GTIN or another identifier;
- **PLU** — another grocery identifier class, especially relevant to produce, and not reducible to GTIN.

A loose apple can also make “one barcode per sellable thing” a bad assumption.

**Provisional Green Grocer implication.** Do not collapse seller-local sellable identity into standardized barcode identity. Whether SKU belongs directly on an `offer`, a `sellable`, or another identifier record remains open.

Sources: [GS1: GTIN, barcode, EAN and UPC](https://support.gs1.org/support/solutions/articles/43000734124-what-is-the-difference-between-a-gs1-gtin-a-barcode-an-ean-and-a-upc-), [Instacart POS transaction fields](https://docs.instacart.com/connect/api/transaction/send_pos_transaction/), [Solidus Variant](https://github.com/solidusio/solidus/blob/main/core/app/models/spree/variant.rb).

The existing fixture note already records the SKU/GTIN distinction and grocery/barcode fixture sources: [references.md](./references.md).

### 10.4 Price is not floating point, and display price is not automatically transaction price

**Observed mature systems.** Spree represents prices as currency-aware price records and line items can lock a price so later catalog price changes do not rewrite the existing line. Solidus calculations use decimal arithmetic rather than binary floating point in its Ruby implementation. Commerce APIs commonly serialize decimal money as decimal values/strings plus currency/display text.

**Interpretation.** At least three questions are hiding inside “price”:

1. **money value** — exact amount in a currency;
2. **pricing basis** — e.g. `$2.99 per lb` versus `$1.49 each`;
3. **price applicability/snapshot** — the currently resolved shopper price versus an amount later committed as part of a transaction.

The shopper UI may display an estimate for variable-weight goods even before exact fulfilled weight exists. That is another reason not to make formatted display text the canonical money value.

**Provisional Green Grocer implication.** Binary floating point is excluded for money. Keep exact money and formatting separate. Investigate whether the shopper cart needs a `price quote`/resolved price concept distinct from a later transaction-confirmed line price; this report does **not** settle when a live cart should reprice.

Sources: [Spree Pricing](https://spreecommerce.org/docs/developer/core-concepts/pricing), [Spree Orders](https://spreecommerce.org/docs/developer/core-concepts/orders), [Solidus calculators/source](https://github.com/solidusio/solidus/tree/main/core/app/models/spree/calculator).

### 10.5 Derived totals

**Observed systems.** Redux explicitly recommends minimal canonical state and selectors for derivations such as sums. Knockout computed observables and modern reactive systems make the same distinction. Spree/Solidus may persist/recalculate totals because they are transactional persistence systems, but that is an implementation/lifecycle choice outside this report's persistence scope.

**Interpretation.** For the shopper model, if a line total is mathematically determined by price basis and requested amount, and a cart subtotal is determined by lines, independently mutable stored copies create consistency obligations for no conceptual gain.

**Provisional Green Grocer implication.** Treat line totals, item/count summaries and merchandise subtotal as **derived concepts by default**. If later profiling/persistence requires caching, cache a derivation without promoting the cache to a second source of truth.

### 10.6 Empty cart

**Observed mature systems.** Spree can create/return an empty cart before any line exists. This matches the ordinary interaction where the shopper has a cart context with zero selections.

**Interpretation.** “No cart exists” and “a valid cart with zero lines exists” are useful distinct possibilities depending on ownership/session design, but there is no semantic reason every cart value must contain a line.

**Provisional Green Grocer implication.** An empty cart should remain a candidate valid cart state. The exact lazy/eager creation policy belongs to application/session design, not the cart-line type.

### 10.7 Zero quantity and remove

**Observed mature systems.** Spree exposes explicit quantity-update and line-removal operations as separate API actions. Square documents zero quantity behavior in some order completion/payment contexts as removing a line. There is therefore no universal mature-system rule saying “zero is always a line” or “zero always means remove” at every layer.

**Interpretation.** Three states should not be confused:

- editor text `"0"`;
- an application intent “set requested quantity to zero”;
- a canonical cart line whose committed requested quantity is zero.

The first two can exist even if the third is forbidden.

**Provisional Green Grocer implication.** A strong candidate is that a canonical cart line represents a **positive** committed request while a UI/application `set to zero` intent may normalize to remove. But this is intentionally **not settled here** because mature systems differ and interaction semantics need an explicit decision in the next job.

### 10.8 Empty quantity input

**Observed across UI/form systems.** Angular/Django/Symfony/Phoenix/React-style forms can hold incomplete invalid input while editing.

**Interpretation.** Empty input is valid editor state and usually invalid committed quantity state.

**Provisional Green Grocer implication.** Do not widen canonical `Quantity` merely to accommodate an HTML/input box between keystrokes. Keep an edit/form representation at the presentation/application-input boundary.

### 10.9 Unavailable quantity

**Observed commerce systems.** Solidus `Variant` contains inventory/supply checks around a requested quantity; Spree exposes in-stock/backorderable behavior. Inventory validation can happen as an order/cart operation progresses.

**Interpretation.** “2 lb” can be a structurally valid requested amount even when only 1 lb is currently available. Availability depends on sellable/inventory state, time, store/location, and policy; those are not intrinsic mathematical properties of the quantity.

**Provisional Green Grocer implication.** Keep inventory availability as a neighboring application concern consulted by add/change operations. An unavailable request can produce a cart-operation failure without making the quantity type itself inventory-aware. Inventory-management UI and fulfillment remain out of scope.

## 11. Architectural ideas worth carrying forward

These are research conclusions strong enough to inform the next job, but they are still architecture criteria rather than final types.

### 11.1 Separate navigation/transport from shopper operations

Supported by Rails/Merb modularization, Roda/Cuba/Syro explicit routing, Hanami single-purpose actions, Trailblazer operations, Express/Koa minimal transport, Phoenix contexts, and Elm/Redux action separation.

A route says **how an external interaction reaches us**. A shopper operation says **what the shopper is trying to do**.

### 11.2 Give state an owner and lifetime

Supported especially by modern Angular DI scopes, React component ownership, Ember's long-lived-controller caution, Redux local-vs-global guidance, LiveView process lifetime, and Hotwire/htmx server-vs-local distinction.

At minimum, investigate separate lifetimes for:

- canonical cart/session state;
- catalog/query/cache state;
- URL route/search state;
- component/form quantity-edit state;
- derived totals (which need no independent mutation lifetime).

### 11.3 Keep edit/form values separate from validated application values

Supported by Django/Symfony/Grails forms, Ecto changesets, Angular forms, React local edit patterns, and the grocery requirement for temporarily incomplete decimal/unit input.

### 11.4 Make derived projections explicitly derived

Supported directly by Redux selectors and Knockout/computed/reactive systems. Cart subtotal/count/line total should begin as projections unless later evidence requires caching.

### 11.5 Keep presentation data explicit enough to protect the domain

Supported by Hanami view separation, Pakyow semantic bindings, Pyramid's view/template distinction, and Web Components/Lit public property boundaries.

A page needs shopper-ready information; that does not imply unrestricted access to persistence/domain internals.

### 11.6 Preserve descriptive product versus sellable proposition as an open but serious distinction

Supported strongly by Spree/Solidus Product versus Variant. The name `Variant` may be too implementation- or option-centric for Green Grocer; `offer` or `sellable` may be better, but this needs the next pass.

### 11.7 Identifier authority must remain visible

Supported by GS1, Spree/Solidus, and Instacart retailer-reference/barcode fields. Seller SKU and GTIN/barcode are not interchangeable.

### 11.8 Grocery quantities need unit semantics

Supported by Square and Instacart. Raw unlabelled numeric quantity is insufficient.

### 11.9 Requested and fulfilled/measured amount are different lifecycle facts

Supported directly by Instacart's `qty_requested` / `qty_fulfilled`. Fulfillment remains outside this report, but the shopper model should not erase the distinction.

### 11.10 Avoid cart → order lifecycle accretion by default

Supported negatively by Spree/Solidus `Order`: convenient shared lifecycle becomes the attachment point for checkout/payment/shipping/returns and other concerns.

### 11.11 Domain values should not be renderer-specific

Supported by every server/client comparison. Money is not `$2.99`; quantity is not input text; cart is not Redux state shape; operation is not an Express handler; route is not a domain identity.

## 12. Ideas explicitly rejected or treated cautiously

### Rejected for this research phase

- **Current repository shape as architecture.** It is accidental history.
- **Current implementation language as a requirement.** None is selected.
- **Rails/Active Record vocabulary as a domain dictionary.** Mature Rails commerce is evidence, not authority.
- **Calling a live cart `Order` by default.** Spree/Solidus show why this deserves skepticism.
- **SKU = UPC/EAN/GTIN/barcode.** The identifier authorities differ.
- **`quantity : number` with no unit semantics.** Grocery evidence directly contradicts this for weighted goods.
- **Floating point money.** Exact monetary arithmetic is required.
- **DOM/input string as canonical cart quantity.** Editing and committed state differ.
- **Independently mutating cart subtotal/item count/line total when fully derivable.** Start from derivation.
- **Template/component receives arbitrary persistence object merely for convenience.** Keep a presentation boundary available.
- **Route handler is necessarily the application operation.** Multiple lineages disprove that necessity.
- **One universal global UI store.** State lifetimes differ.
- **One universal component-local state rule.** Cart must span pages/components; some state genuinely is shared.
- **SPA as an assumption.** Hotwire/htmx demonstrate a coherent server-driven alternative.
- **Server-driven HTML as an assumption.** Elm/React/Angular demonstrate coherent explicit browser-state alternatives.

### Treated cautiously rather than rejected

- **`Product` / `Variant`.** The distinction has strong evidence; the names may be wrong for Green Grocer.
- **`Offer`.** It may better express a seller-specific sellable proposition, but we have not yet proven it needs to be first-class rather than a projection of product + sale terms.
- **`CartLine`.** Mature commerce uses line items, but exact identity semantics remain open: does one sellable always correspond to one line, and what future options/customizations would split lines?
- **`PriceQuote`.** Conceptually promising for current shopper-resolved price versus later confirmed transaction price, but repricing rules are not researched enough to settle it.
- **`BuyerAction` / `Intent` / `Command` / `Operation`.** Multiple architectures support the role; naming and granularity remain open.
- **`PageData` / `ViewModel` / `Presenter`.** An explicit presentation boundary is useful, but one universal DTO layer can become boilerplate. The next job should decide whether page-specific values, selectors, or presenter functions are enough.
- **Canonical cart line quantity strictly positive.** Strong candidate; mature systems differ in how “set zero” is interpreted at APIs, so leave it open pending explicit interaction semantics.

## 13. Open questions

These are the questions the type-design job should answer rather than inherit from a framework.

1. **What exactly is the sellable thing?** Is a descriptive product ever directly sellable, or is there always a seller-specific offer/sellable identity between product and cart line?
2. **Is `Variant` semantically wrong when there is no visible variation?** Would `offer`, `item`, `sellable`, or another term communicate the role better?
3. **What owns SKU?** Product, sellable offer, seller catalog entry, or a separate seller-assigned identifier object?
4. **Where does GTIN belong?** On a trade-item/package identity that a sellable offer references? How are loose produce/PLU cases represented?
5. **What is the quantity algebra?** Separate count/weight cases, a dimensional quantity, or an offer-specific requested amount validated against a selling-unit policy?
6. **How is approximate requested weight represented?** Is “about 3 lb” encoded in quantity semantics, seller rounding policy, or merely presentation? Instacart proves requested and fulfilled differ but does not settle the best internal representation.
7. **What is a selling/pricing unit?** `each`, `lb`, package, bunch, etc. Does it belong on the sellable offer, the price, both through one shared unit concept, or elsewhere?
8. **What is a shopper-visible price?** Is it exact `Money + pricing basis`, a market-resolved quote with validity, or a page projection?
9. **When does a live cart reprice?** On every view, on add/change, never until explicit refresh, or according to quote validity? Spree's locked line price is evidence but not automatically our policy.
10. **How should estimated totals for variable-weight goods be labeled?** A 3 lb request fulfilled at 3.12 lb means a pre-fulfillment subtotal may be estimate rather than final transaction amount.
11. **What gives a cart line identity?** Sellable identity alone, or sellable + selected options/instructions/unit choice? This matters for “add same thing twice.”
12. **Is zero a valid committed line quantity?** Or should `set 0` normalize to remove before canonical cart state?
13. **Can an empty cart exist independently of a shopper/session locator?** This is an application ownership question, not just a collection question.
14. **What operation failures can the cart layer produce?** Invalid requested amount, offer not sellable, insufficient availability, stale price, etc. Which belong to cart versus neighboring services?
15. **How much inventory knowledge may an add/change operation consult?** The inventory-management system is out of scope, but the boundary contract is not.
16. **What navigation state is canonical?** Product ID, category/search/filter/sort, return/continue-shopping location; which belongs in URL versus transient UI?
17. **How are catalog state and cart state synchronized if renderer/transport is remote?** The domain concepts should not choose deployment, but concurrency/version results may later affect operation result shape.
18. **What is the smallest useful presentation boundary?** Page-specific records, presenter functions, selectors, or components consuming a stable public domain projection?
19. **Should shopper intents be nouns/events (`QuantityChanged`) or imperative operations (`ChangeQuantity`)?** Different architectures suggest both; the semantic distinction between attempted command and observed event may matter.
20. **What invariants deserve static representation versus runtime validation?** Unit dimension and exact money might be statically strong; current stock cannot be.

## 14. Candidate type boundaries — explicitly provisional

This table is a research handoff, **not** a declaration of the Green Grocer type system.

| Candidate concept | Research support | Competing representation | What remains uncertain |
|---|---|---|---|
| Descriptive product/catalog product | Spree/Solidus separate Product from sellable Variant; shopper pages need shared descriptive identity | Collapse directly into sellable item for a very small catalog | Whether Green Grocer ever needs product grouping independent of sellable entry |
| Sellable offer / sellable item | Spree/Solidus Variant owns SKU/price/inventory-facing concerns; generic product is not exact purchasable unit | Call it Variant; product itself sellable; seller catalog entry | Best name; whether offer includes price/unit/availability or references them |
| Seller SKU | Commerce systems + retailer APIs distinguish local catalog identity | Plain string field on sellable | Whether a dedicated scoped identifier type is useful; uniqueness scope |
| Trade-item/barcode identity | GS1 distinguishes GTIN; grocery also has PLU/barcode cases | Optional string(s) on sellable | Package level, multiple codes, loose produce, barcode carrier vs encoded identifier |
| Cart | Common across systems; can be empty; shopper state before checkout | Reuse `Order` lifecycle as Spree/Solidus do | Ownership/lifetime/session locator; repricing policy |
| Cart line | Mature commerce represents sellable + quantity + price context per selection | Map from sellable ID directly to quantity; no line identity | Line identity/merging; future instructions/options; price snapshot |
| Requested amount / quantity | Square/Instacart require unit semantics; Instacart separates requested/fulfilled | Generic numeric `quantity`; count/weight enum; dimensional quantity | Exact algebra, precision/step, approximate intent |
| Selling unit / quantity policy | Each vs weight and price-per-unit require it | Embed unit inside quantity only; embed in price only | Whether same unit concept drives validation and pricing; package/bunch edge cases |
| Money | Exact price required; mature commerce uses decimal/currency | Integer minor units; arbitrary precision decimal + currency | Internal representation and currencies supported; outside scope of report to choose implementation |
| Shopper price / price quote | Spree current price vs locked line price demonstrates lifecycle distinction | One `Price` value everywhere | Repricing/validity semantics; estimate label for variable weight |
| Shopper intent / application operation | Trailblazer, Hanami, Redux/Elm, Phoenix contexts all separate use-case transition from transport | Controller/route method directly owns behavior | Naming, granularity, command/event distinction, sync/async result |
| Shopper/cart application state | Angular/React/Redux/LiveView all require explicit ownership | One global store or one server session object | Lifetime, concurrency, local/remote implementation |
| Route/navigation state | Backbone/Angular/Ember/React routers/LiveView URL params separate navigable state | Fold current page/search into UI component state only | Which filters/search/return location must be URL-addressable |
| Quantity edit/form state | Django/Symfony/Angular/Ecto + normal browser editing support incomplete invalid values | Widen canonical quantity to permit invalid/empty | Exact form representation; when validation runs; locale/unit entry |
| Page/view/presentation data | Hanami/Pakyow/Pyramid/Lit support presentation boundary | Templates/components receive domain objects directly | How explicit to make it without duplicative DTO boilerplate |
| Operation result/failure | hapi/form systems/Trailblazer/Phoenix distinguish interface and application failure | Exceptions/boolean everywhere | Failure vocabulary and which errors belong to cart vs inventory/catalog neighbor |

### Important negative boundary

A later **fulfilled/measured quantity** is deliberately **not** proposed as a shopper-cart type here. Instacart proves the distinction exists, but fulfillment is outside scope. The shopper-side design only needs to avoid making that later fact impossible to represent cleanly.

## 15. What research remains before settling the type system

The broad framework archaeology is sufficient to stop inheriting a framework ontology blindly, but several focused investigations should happen before final types are declared.

1. **Grocery quantity semantics beyond two examples.** Instacart and Square strongly establish unit-aware and requested-vs-fulfilled concepts. Check another grocery/POS/catalog system for variable-weight pricing, rounding increments, packages/bunches, and PLUs.
2. **Offer/product vocabulary outside Rails commerce.** Compare at least one non-Spree mature commerce model (for example commercetools, Shopify, Medusa, Saleor, or an ERP/POS catalog) specifically on product/variant/offer/catalog-entry boundaries. The goal is naming/boundary evidence, not adopting their whole architecture.
3. **Live-cart repricing semantics.** Compare how mature systems handle catalog price changes between add and checkout. This is necessary before deciding whether a cart line stores a quote/snapshot or references a currently resolved offer price.
4. **Grocery estimated subtotal semantics.** Research how variable-weight stores label estimated versus final totals before designing any `EstimatedMoney`-like concept.
5. **Cart-line identity/merging.** Examine how systems decide whether adding the same sellable increases one line or creates another, especially when unit choice or shopper instructions exist.
6. **Quantity edit UX as an architecture pressure test.** Test mobile/keyboard behavior for decimal weight, clear/retype, increment buttons, and remove-via-zero. This can settle whether edit state needs a first-class representation.
7. **Presentation boundary experiment.** Sketch one catalog page and one cart page using explicit page data, then repeat with direct domain-object rendering. Measure duplication/leakage rather than assuming a DTO layer is good.
8. **Transport swap exercise.** Express the same operations from a server HTML handler, a JSON endpoint, and a local/native UI call. Anything that cannot survive the swap is probably transport-specific.
9. **Only then settle the type vocabulary.** The next job can decide whether the final words are `product`, `offer`, `cart`, `cart line`, `requested amount`, `money`, `price quote`, `shopper intent`, `route`, `page data`, etc.

The existing grocery fixture/mock sources should remain available for realistic test data but should not be treated as architecture authorities: [references.md](./references.md).

## 16. Source links

### Ruby / Rails lineage

- Rails Getting Started — <https://guides.rubyonrails.org/getting_started.html>
- Rails Action View Overview — <https://guides.rubyonrails.org/action_view_overview.html>
- Rails 3 release notes — <https://guides.rubyonrails.org/3_0_release_notes.html>
- Yehuda Katz, “Rails and Merb Merge” — <https://yehudakatz.com/2008/12/23/rails-and-merb-merge/>
- Rails, “Merb gets merged into Rails 3” — <https://rubyonrails.org/2008/12/23/merb-gets-merged-into-rails-3>
- Rack — <https://github.com/rack/rack>
- Camping — <https://github.com/camping/camping>
- Sinatra — <https://sinatrarb.com/intro.html>
- Ramaze project/readme — <https://www.ruby-toolbox.com/projects/ramaze>
- Ramaze 2009 announcement — <https://rubytalk.org/t/ann-ramaze-2009-06/53691>
- Cuba — <https://cuba.is/>
- Syro — <https://github.com/soveran/syro>
- Roda — <https://github.com/jeremyevans/roda>
- Scorched — <https://scorchedrb.com/>
- Padrino — <https://github.com/padrino/padrino-framework>
- Hanami — <https://github.com/hanami/hanami>
- Hanami Action — <https://github.com/hanami/hanami-action>
- Hanami Router — <https://github.com/hanami/router>
- Hanami View — <https://github.com/hanami/view>
- Pakyow — <https://github.com/pakyow/pakyow>
- Grape — <https://github.com/ruby-grape/grape>
- Trailblazer — <https://github.com/trailblazer/trailblazer>
- Trailblazer Operations — <https://trailblazer.to/2.1/docs/operation/>
- InfoQ, “Forgotten Ruby Web Frameworks” (Nitro and peers) — <https://www.infoq.com/news/2007/11/forgotten-ruby-web-frameworks/>
- InfoQ, Waves creator interview — <https://www.infoq.com/news/2008/02/waves-ruby-framework/>
- Ruby Archaeology: Forgotten Web Frameworks — <https://www.rubyevents.org/talks/ruby-archaeology-forgotten-web-frameworks>

### Other Rails-era server frameworks

- Django URL dispatcher — <https://docs.djangoproject.com/en/stable/topics/http/urls/>
- Django forms — <https://docs.djangoproject.com/en/stable/topics/forms/>
- Pylons documentation — <https://docs.pylonsproject.org/projects/pylons-webframework/en/latest/>
- Pyramid introduction/history — <https://docs.pylonsproject.org/projects/pyramid/en/latest/narr/introduction.html>
- Pyramid design defense — <https://docs.pylonsproject.org/projects/pyramid/en/latest/designdefense.html>
- TurboGears — <https://turbogears.org/>
- The TurboGears Way — <https://turbogears.org/welcome/turbogears-way.html>
- CakePHP documentation — <https://book.cakephp.org/>
- Grails documentation — <https://docs.grails.org/>
- Play Framework 1 FAQ — <https://www.playframework.com/documentation/1.0/faq>
- Symfony routing — <https://symfony.com/doc/current/routing.html>
- Symfony service container — <https://symfony.com/doc/current/service_container.html>
- Symfony forms — <https://symfony.com/doc/current/forms.html>
- Symfony validation — <https://symfony.com/doc/current/validation.html>

### Node / JavaScript server lineage

- Express — <https://expressjs.com/>
- Connect — <https://github.com/senchalabs/connect>
- Koa — <https://koajs.com/>
- hapi — <https://hapi.dev/>
- Sails.js — <https://sailsjs.com/>
- Meteor — <https://docs.meteor.com/>

### Elixir / Phoenix

- Phoenix controllers — <https://hexdocs.pm/phoenix/controllers.html>
- Phoenix contexts — <https://hexdocs.pm/phoenix/contexts.html>
- Ecto.Changeset — <https://hexdocs.pm/ecto/Ecto.Changeset.html>
- Phoenix LiveView — <https://hexdocs.pm/phoenix_live_view/Phoenix.LiveView.html>
- LiveView forms — <https://hexdocs.pm/phoenix_live_view/form-bindings.html>

### Browser/interface architecture

- jQuery — <https://jquery.com/>
- Backbone — <https://backbonejs.org/>
- Knockout — <https://knockoutjs.com/>
- AngularJS archive — <https://code.angularjs.org/>
- Modern Angular — <https://angular.dev/overview>
- Angular dependency injection — <https://angular.dev/guide/di>
- Angular forms — <https://angular.dev/guide/forms>
- Angular routing — <https://angular.dev/guide/routing>
- Ember Guides — <https://guides.emberjs.com/>
- React Managing State — <https://react.dev/learn/managing-state>
- Flux in-depth overview — <https://facebookarchive.github.io/flux/docs/in-depth-overview/>
- Redux fundamentals — <https://redux.js.org/tutorials/fundamentals/part-1-overview>
- Redux selectors / derived data — <https://redux.js.org/usage/deriving-data-selectors>
- Vue state management — <https://vuejs.org/guide/scaling-up/state-management.html>
- Svelte docs — <https://svelte.dev/docs>
- SvelteKit form actions — <https://svelte.dev/docs/kit/form-actions>
- Elm Architecture — <https://guide.elm-lang.org/architecture/>
- Solid signals — <https://docs.solidjs.com/concepts/signals>
- Stimulus — <https://stimulus.hotwired.dev/>
- Turbo introduction — <https://turbo.hotwired.dev/handbook/introduction>
- htmx hypermedia-friendly scripting — <https://htmx.org/essays/hypermedia-friendly-scripting/>
- MDN Web Components — <https://developer.mozilla.org/en-US/docs/Web/API/Web_components>
- Lit properties/state — <https://lit.dev/docs/components/properties/>

### Commerce / grocery semantics

- Spree products — <https://spreecommerce.org/docs/developer/core-concepts/products>
- Spree pricing — <https://spreecommerce.org/docs/developer/core-concepts/pricing>
- Spree orders — <https://spreecommerce.org/docs/developer/core-concepts/orders>
- Spree cart API — <https://spreecommerce.org/docs/api-reference/storefront/carts/create-a-cart>
- Spree line quantity API — <https://spreecommerce.org/docs/api-reference/storefront/cart-line-items/set-line-item-quantity>
- Solidus Product source — <https://github.com/solidusio/solidus/blob/main/core/app/models/spree/product.rb>
- Solidus Variant source — <https://github.com/solidusio/solidus/blob/main/core/app/models/spree/variant.rb>
- Solidus Order source — <https://github.com/solidusio/solidus/blob/main/core/app/models/spree/order.rb>
- Square OrderLineItem — <https://developer.squareup.com/reference/square/objects/OrderLineItem>
- Instacart product quantity types — <https://docs.instacart.com/connect/fulfillment_guide/concepts/product_quantity>
- Instacart create delivery order / requested and fulfilled quantities — <https://docs.instacart.com/connect/api/fulfillment/delivery/create_order/>
- Instacart POS transaction fields — <https://docs.instacart.com/connect/api/transaction/send_pos_transaction/>
- GS1: GTIN, barcode, EAN, UPC — <https://support.gs1.org/support/solutions/articles/43000734124-what-is-the-difference-between-a-gs1-gtin-a-barcode-an-ean-and-a-upc->

### Green Grocer fixture references retained separately

- [Green Grocer fixture/reference note](./references.md)
- Open Science Online Grocery — <https://github.com/Open-Science-Online-Grocery/online-grocery>
- esther-ng/groceries — <https://github.com/esther-ng/groceries>
- Glovo Hackathon AI Summit 2025 mock grocery data — <https://github.com/Glovo/Hackathon-AI-Summit-2025>
- EventideSystems/brocade.io barcode-product fixtures — <https://github.com/EventideSystems/brocade.io>

---

## Research handoff

The next job should **not** begin by coding these candidate types. It should begin by taking the open questions and candidate-boundary table above and deciding which distinctions earn a place in the shopper-facing/nearest-neighbor vocabulary.

The strongest evidence from this pass is structural rather than nominal:

- shopper operations can exist independently of routes/renderers;
- state has multiple lifetimes and should have explicit ownership;
- edit state is not canonical cart state;
- derivable totals need not be independently mutable facts;
- grocery quantity needs units and requested/fulfilled distinction;
- seller SKU and standardized trade identity differ;
- descriptive product and exact sellable identity are often distinct in mature commerce systems;
- a live cart need not inherit the later order/checkout lifecycle;
- presentation should receive what it needs without dictating domain/persistence shape.

Everything more specific than that remains deliberately open for the type-design phase.